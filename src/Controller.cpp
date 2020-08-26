#include "Controller.hpp"

namespace fc {
milliseconds update_interval(500);
bool dynamic = true;
uint smoothing_intervals = 4;
uint top_stickiness_intervals = 4;
uint temp_averaging_intervals = 8;
} // namespace fc

fc::Controller::Controller(path conf_path_) : config_path(move(conf_path_)) {
  load_conf_and_enumerated();
  watcher = spawn_watcher();
}

fc::Controller::~Controller() { disable_all(); }

FanStatus fc::Controller::status(const string &flabel) const {
  const auto it = fthreads.find(flabel);
  if (it == fthreads.end())
    return FanStatus::FanStatus_Status_DISABLED;

  return (it->second.is_testing()) ? FanStatus::FanStatus_Status_TESTING
                                   : FanStatus::FanStatus_Status_ENABLED;
}

void fc::Controller::enable(fc::FanInterface &f, bool enable_all_dell) {
  if (fthreads.count(f.label) > 0) {
    LOG(llvl::trace) << f << ": already enabled";
    return;
  }

  // Dell fans can only be enabled/disabled togther
  if (f.type() == DevType::DELL && enable_all_dell)
    return enable_dell_fans();

  if (!f.pre_start_check())
    return;

  fthreads.try_emplace(f.label, [this, &f](bool &run) {
    while (run) {
      notify_status_observers(f.label);
      f.update();
    }
  });

  LOG(llvl::trace) << f.label << ": enabled";
}

void fc::Controller::enable_all() {
  for (auto &[key, f] : devices.fans) {
    if (!fthreads.contains(f->label))
      enable(*f);
  }
}

void fc::Controller::disable(const string &flabel, bool disable_all_dell) {
  const auto it = fthreads.find(flabel);
  if (it == fthreads.end()) {
    LOG(llvl::error) << flabel << ": failed to find to disable";
    return;
  }

  // Dell fans can only be enabled/disabled togther
  if (devices.fans.at(flabel)->type() == DevType::DELL && disable_all_dell)
    return disable_dell_fans();

  fthreads.erase(it);
  if (const auto fit = devices.fans.find(flabel); fit != devices.fans.end())
    fit->second->disable_control();

  LOG(llvl::trace) << flabel << ": disabled";
  notify_status_observers(flabel);
}

void fc::Controller::disable_all() {
  vector<string> disabled_fans;
  for (const auto &[flabel, fthread] : fthreads)
    disabled_fans.push_back(flabel);

  fthreads.clear();
  for (const auto &flabel : disabled_fans)
    notify_status_observers(flabel);

  LOG(llvl::trace) << "Disabling all";
}

void fc::Controller::reload() {
  // Remove all threads not testing
  vector<string> stopped;
  for (auto &[key, fthread] : fthreads) {
    if (!fthread.is_testing())
      stopped.push_back(key);
  }

  // TODO: fix devices being overwritten during testing; causing SEGFAULT!
  // TODO: reload ONLY modified - side-stepping this issue
  // stop all, then restart the fans currently testing?
  for (const string &key : stopped)
    fthreads.erase(key);

  // Restart
  LOG(llvl::info) << "Reloading changes";
  load_conf_and_enumerated();
  enable_all();
}

void fc::Controller::reload_added() {
  // TODO:
  // Load config changes
  load_conf_and_enumerated();
  // Compare new devices to old
  // Interrupt and enable ed
  // Interrupt all threads not testing, and don't wait
  for (auto &[key, fthread] : fthreads) {
    if (!fthread.is_testing())
      fthread.t.interrupt();
  }
}

void fc::Controller::nv_init() {
#ifdef FANCON_NVIDIA_SUPPORT
  if (NV::init(true))
    reload(); // TODO: reload only added devices
#endif        // FANCON_NVIDIA_SUPPORT
}

void fc::Controller::test(fc::FanInterface &fan, bool forced,
                          function<void(int &)> cb) {
  if (fan.ignore || (fan.tested() && !forced))
    return;

  // If a test is already running for the device then just join onto it
  if (auto it = fthreads.find(fan.label); it->second.is_testing()) {
    it->second.test_status->register_observer(cb);
    it->second.join();
    return;
  }

  LOG(llvl::info) << fan << ": testing";

  // Remove any running thread before testing
  if (const auto it = fthreads.find(fan.label); it != fthreads.end())
    fthreads.erase(it);

  auto test_status = make_shared<Observable<int>>(0);
  test_status->register_observer(cb);

  auto test_func = [&] {
    // Test fan, then remove thread from map
    fan.test(*test_status);
    fthreads.erase(fan.label);
    LOG(llvl::info) << fan << ": test complete";

    // Only write to file when no other fan are still testing
    if (tests_running() == 0)
      to_file(false);

    enable(fan);
  };

  auto [it, success] =
      fthreads.try_emplace(fan.label, move(test_func), test_status);

  if (!success)
    test(fan, forced, cb);

  notify_status_observers(fan.label);
  it->second.join();
}

size_t fc::Controller::tests_running() const {
  auto f = [](const size_t sum, const auto &p) {
    return sum + int(p.second.is_testing());
  };
  return std::accumulate(fthreads.begin(), fthreads.end(), 0, f);
}

void fc::Controller::set_devices(const fc_pb::Devices &devices_) {
  disable_all();
  devices = fc::Devices(false);
  devices.from(devices_);
  to_file(false);
  enable_all();
}

void fc::Controller::from(const fc_pb::Controller &c) {
  from(c.config());
  fc::Devices import_devices;
  import_devices.from(c.devices());

  for (auto &[key, f] : import_devices.fans) {
    // Insert or assign or overwrite any existing device sharing that hw_id
    const string hw_id = f->hw_id();
    auto it = find_if(devices.fans.begin(), devices.fans.end(),
                      [&](auto &p) { return p.second->hw_id() == hw_id; });
    if (it != devices.fans.end())
      it->second = move(f);
    else
      devices.fans.insert_or_assign(key, move(f));
  }

  for (auto &[key, s] : import_devices.sensors) {
    const string hw_id = s->hw_id();
    auto it = find_if(devices.sensors.begin(), devices.sensors.end(),
                      [&](auto &p) { return p.second->hw_id() == hw_id; });
    if (it != devices.sensors.end())
      it->second = move(s);
    else
      devices.sensors.insert_or_assign(key, move(s));
  }
}

void fc::Controller::from(const fc_pb::ControllerConfig &c) {
  update_interval = milliseconds(c.update_interval());
  dynamic = c.dynamic();
  smoothing_intervals = c.smoothing_intervals();
  top_stickiness_intervals = c.top_stickiness_intervals();
  temp_averaging_intervals = c.temp_averaging_intervals();
}

void fc::Controller::to(fc_pb::Controller &c) const {
  to(*c.mutable_config());
  devices.to(*c.mutable_devices());
}

void fc::Controller::to(fc_pb::ControllerConfig &c) const {
  c.set_update_interval(update_interval.count());
  c.set_dynamic(dynamic);
  c.set_smoothing_intervals(smoothing_intervals);
  c.set_top_stickiness_intervals(top_stickiness_intervals);
  c.set_temp_averaging_intervals(temp_averaging_intervals);
}

void fc::Controller::enable_dell_fans() {
  for (const auto &[fl, f] : devices.fans) {
    if (f->type() == DevType::DELL)
      enable(*f, false);
  }
}

void fc::Controller::disable_dell_fans() {
  for (const auto &[fl, f] : devices.fans) {
    if (f->type() == DevType::DELL)
      disable(fl, false);
  }
}

void fc::Controller::load_conf_and_enumerated() {
  devices = fc::Devices(true);

  if (config_path.empty() || !exists(config_path)) {
    LOG(llvl::debug) << "No config found";
    return;
  }

  std::ifstream ifs(config_path);
  std::stringstream ss;
  ss << ifs.rdbuf();

  fc_pb::Controller c;
  google::protobuf::TextFormat::ParseFromString(ss.str(), &c);

  if (ifs) {
    update_config_write_time();
    from(c);
    notify_devices_observers();
  } else {
    LOG(llvl::error) << "Failed to read config from: " << config_path;
  }
}

void fc::Controller::to_file(bool backup) {
  // Backup existing file
  if (backup && exists(config_path)) {
    const path backup_path = config_path.string() + "-" + date_time_now();
    fs::rename(config_path, backup_path);
    LOG(llvl::info) << "Moved previous config to: " << backup_path;
  }

  std::ofstream ofs(config_path);
  fc_pb::Controller c;
  to(c);

  string out_s;
  google::protobuf::TextFormat::Printer printer;
  printer.SetUseShortRepeatedPrimitives(true);

  printer.PrintToString(c, &out_s);
  ofs << out_s;

  if (ofs) {
    update_config_write_time();
    notify_devices_observers();
  } else {
    LOG(llvl::error) << "Failed to write config to: " << config_path;
  }
}

void fc::Controller::update_config_write_time() {
  if (exists(config_path))
    config_write_time = fs::last_write_time(config_path);
}

bool fc::Controller::config_file_modified() const {
  if (!exists(config_path))
    return false;

  return config_write_time != fs::last_write_time(config_path);
}

thread fc::Controller::spawn_watcher() {
  return thread([this] {
    while (true) {
      if (config_file_modified())
        reload();
      sleep_for(update_interval);
    }
  });
}

void fc::Controller::notify_devices_observers() {
  if (device_observers.empty())
    return;

  const auto scoped_lock = device_observers_mutex.acquire_lock();
  for (const auto &f : device_observers)
    f(devices);
}

void fc::Controller::notify_status_observers(const string &flabel) {
  if (status_observers.empty())
    return;

  const auto fit = devices.fans.find(flabel);
  if (fit == devices.fans.end())
    return;

  const FanStatus fstatus = status(flabel);
  const auto scoped_lock = status_observers_mutex.acquire_lock();
  for (const auto &f : status_observers)
    f(*fit->second, fstatus);
}

string fc::Controller::date_time_now() {
  std::time_t tt = chrono::system_clock::to_time_t(chrono::system_clock::now());
  std::tm tm{};
  gmtime_r(&tt, &tm);

  std::stringstream ss;
  ss << tm.tm_year << "-" << tm.tm_mon << "-" << tm.tm_mday << "-" << tm.tm_hour
     << "-" << tm.tm_min;
  return ss.str();
}
