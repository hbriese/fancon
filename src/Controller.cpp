#include "Controller.hpp"

namespace fc {
bool dynamic = true;
uint smoothing_intervals = 3;
uint top_stickiness_intervals = 2;
uint temp_averaging_intervals = 3;
} // namespace fc

fc::FanThread &fc::FanThread::operator=(fc::FanThread &&other) noexcept {
  t = move(other.t);
  test_status = other.test_status;
  return *this;
}

fc::Controller::Controller(path conf_path_)
    : state(ControllerState::DISABLED), config_path(move(conf_path_)) {
  load_conf_and_enumerated();
}

fc::Controller::~Controller() { disable(); }

void fc::Controller::control(fc::FanInterface &f) {
  if (threads.count(f.label) > 0) {
    LOG(llvl::debug) << f << ": duplicate thread";
    return;
  }

  if (!f.pre_start_check())
    return;

  auto update_func = [this, &f]() {
    while (enabled()) {
      f.update();
      sleep_for(update_interval);
    }
    threads.erase(f.label);
  };

  threads.try_emplace(f.label, thread(update_func), nullptr);
}

void fc::Controller::test(fc::FanInterface &fan, bool forced,
                          function<void(int &)> cb) {
  if (fan.ignore || (fan.tested() && !forced))
    return;

  // If a test is already running for the device then just join onto it
  if (auto it = threads.find(fan.label); it->second.is_testing()) {
    it->second.test_status->register_observer(cb);
    it->second.join();
    return;
  }

  LOG(llvl::info) << fan << ": testing";

  // Remove any running thread before testing
  if (auto it = threads.find(fan.label); it != threads.end())
    threads.erase(it);

  auto test_status = make_shared<Observable<int>>(0);
  test_status->register_observer(cb);

  auto test_func = [&] {
    // Test fan, then remove thread from map
    fan.test(*test_status);
    threads.erase(fan.label);
    LOG(llvl::info) << fan << ": test complete";

    // TODO: use locks to avoid race condition
    if (tests_running() == 0)
      to_file();

    control(fan);
  };

  auto [it, success] =
      threads.try_emplace(fan.label, thread(test_func), test_status);

  if (!success)
    test(fan, forced, cb);

  it->second.join();
}

bool fc::Controller::testing(const string &label) const {
  auto it = threads.find(label);
  return (it != threads.end()) && it->second.is_testing();
}

size_t fc::Controller::tests_running() const {
  auto f = [](const size_t sum, const auto &p) {
    return sum + int(p.second.is_testing());
  };
  return std::accumulate(threads.begin(), threads.end(), 0, f);
}

void fc::Controller::enable() {
  state = ControllerState::ENABLED;
  do {
    for (auto &[key, f] : devices.fans)
      control(*f);

    if (threads.empty())
      LOG(llvl::info) << "Awaiting configuration";

    while (enabled()) {
      if (config_file_modified()) {
        reload();
      } else {
        sleep_for(update_interval);
      }
    }

    if (reloading()) {
      state = ControllerState::ENABLED;
      LOG(llvl::info) << "Reloading changes";
      load_conf_and_enumerated();
    }
  } while (enabled());
}

bool fc::Controller::enabled() const {
  return state == ControllerState::ENABLED;
}

void fc::Controller::disable() {
  state = ControllerState::DISABLED;

  // Interrupt all threads, and wait for them to finish
  threads.clear();
  join_threads();
}

bool fc::Controller::disabled() const {
  return state == ControllerState::DISABLED;
}

void fc::Controller::reload() {
  state = ControllerState::RELOAD;

  // Interrupt all threads not testing, and don't wait
  vector<string> to_remove;
  for (auto &[key, fthread] : threads) {
    if (!fthread.is_testing())
      to_remove.push_back(key);
  }

  for (const string &key : to_remove)
    threads.erase(key);
}

void fc::Controller::reload_added() {
  // TODO:
  // Load config changes
  load_conf_and_enumerated();
  // Compare new devices to old
  // Interrupt and enable ed
  // Interrupt all threads not testing, and don't wait
  for (auto &[key, fthread] : threads) {
    if (!fthread.is_testing())
      fthread.t.interrupt();
  }
}

bool fc::Controller::reloading() const {
  return state == ControllerState::RELOAD;
}

void fc::Controller::nv_init() {
#ifdef FANCON_NVIDIA_SUPPORT
  if (NV::init(true))
    reload(); // TODO: just reload added devices
#endif        // FANCON_NVIDIA_SUPPORT
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

void fc::Controller::load_conf_and_enumerated() {
  devices = fc::Devices(false);

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
    from(c);
    update_config_write_time();
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
  google::protobuf::TextFormat::PrintToString(c, &out_s);
  ofs << out_s;

  if (ofs) {
    update_config_write_time();
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

void fc::Controller::join_threads() {
  for (auto &[key, ft] : threads) { // Wait for all threads to shutdown
    if (ft.t.joinable())
      ft.t.join();
  }
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
