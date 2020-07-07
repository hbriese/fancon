#include "Controller.hpp"

namespace fc {
bool dynamic = true;
uint smoothing_intervals = 3;
uint top_stickiness_intervals = 2;
uint temp_averaging_intervals = 3;
ControllerState controller_state = ControllerState::RUN;
} // namespace fc

fc::Controller::Controller(path conf_path_) : config_path(move(conf_path_)) {
  load_devices();
}

void fc::Controller::start() {
  LOG(llvl::info) << "Starting controller";
  do {
    test(false);

    vector<thread> threads;
    for (unique_ptr<FanInterface> &f : devices.fans) {
      if (f->ignore) {
        LOG(llvl::debug) << *f << ": ignored, skipping";
        continue;
      }

      if (!f->configured())
        continue;

      if (!f->enable_control()) {
        LOG(llvl::error) << *f << ": failed to take control";
        continue;
      }

      threads.emplace_back([this, &f]() {
        while (running()) {
          f->update();
          sleep_for(update_interval);
        }
      });
    }

    if (devices.fans.empty()) {
      LOG(llvl::info) << "No devices detected, exiting";
      return;
    }

    if (threads.empty())
      LOG(llvl::info) << "Awaiting configuration";

    while (running()) { // Restart controller on external config changes
      if (config_file_modified()) {
        reload();
      } else {
        sleep_for(milliseconds(500));
      }
    }

    shutdown_threads(threads);

    // Reload devices only if requested & changes have actually been made
    if (reloading()) {
      controller_state = ControllerState::RUN;
      if (config_file_modified()) {
        LOG(llvl::info) << "Reloading changes";
        load_devices();
      }
    }
  } while (running());
}

void fc::Controller::test(bool force, bool safely) {
  vector<decltype(devices.fans.begin())> to_test;
  for (auto it = devices.fans.begin(); it != devices.fans.end(); ++it) {
    if ((!(*it)->tested() || force) && !(*it)->ignore)
      to_test.emplace_back(it);
  }

  if (to_test.empty())
    return;

  LOG(llvl::info) << "Testing fans, this may take a long time." << endl
                  << "Don't stress your system!";

  // Run only one test at a time if run 'safely'
  const double per_test_percent = 100.0 / to_test.size();
  while (!to_test.empty()) {
    vector<thread> threads;
    vector<shared_ptr<double>> test_completion;
    if (!safely) {
      threads.reserve(to_test.size());
      test_completion.reserve(to_test.size());
    }

    for (auto it = to_test.begin(); it != to_test.end();) {
      auto completed = make_shared<double>(0.0);
      auto &f = **it;
      LOG(llvl::info) << *f << ": testing";
      threads.emplace_back([&f, completed] { f->test(completed); });
      test_completion.emplace_back(completed);

      it = to_test.erase(it);
      if (safely)
        break;
    }

    double percent, prev_percent = -1;
    while (running() && tests_running(test_completion)) {
      percent = per_test_percent * sum(test_completion);
      if (percent != prev_percent) {
        prev_percent = percent;
        cout << "\rTests " << percent << "% complete" << std::flush;
      }

      sleep_for(milliseconds(500));
    }

    shutdown_threads(threads);
  }
  cout << endl;

  if (running()) {
    LOG(llvl::info) << "Testing complete. Configure devices at: "
                    << config_path;
    to_file(config_path, true);
  } else {
    LOG(llvl::info) << "Testing cancelled";
  }
}

bool fc::Controller::running() {
  return controller_state == ControllerState::RUN;
}

void fc::Controller::stop() { controller_state = ControllerState::STOP; }

void fc::Controller::reload() { controller_state = ControllerState::RELOAD; }

bool fc::Controller::reloading() {
  return controller_state == ControllerState::RELOAD;
}

void fc::Controller::reload_nvidia() {
#ifdef FANCON_NVIDIA_SUPPORT
  NV::init(true);
  reload();
#endif // FANCON_NVIDIA_SUPPORT
}

void fc::Controller::from(const fc_pb::Controller &c) {
  from(c.config());
  fc::Devices import_devices;
  import_devices.from(c.devices());

  for (unique_ptr<FanInterface> &f : import_devices.fans) {
    auto it = find_if(devices.fans.begin(), devices.fans.end(),
                      [&f](auto &target) { return target->uid() == f->uid(); });
    if (it != devices.fans.end()) {
      *it = move(f);
    } else {
      devices.fans.emplace_back(move(f));
    }
  }

  for (auto &p : import_devices.sensor_map) {
    auto it = find_if(
        devices.sensor_map.begin(), devices.sensor_map.end(),
        [&p](const auto &t) { return t.second->uid() == p.second->uid(); });
    if (it != devices.sensor_map.end()) {
      it->second = p.second;
    } else {
      devices.sensor_map.emplace(p.first, p.second);
    }
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

void fc::Controller::load_devices() {
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

void fc::Controller::to_file(path file, bool backup) {
  // Backup existing file
  if (backup && exists(file)) {
    const path new_p(file.string() + "-" + date_time_now());
    LOG(llvl::info) << "Moving previous config to: " << new_p;
    fs::rename(file, new_p);
  }

  std::ofstream ofs(file);
  fc_pb::Controller c;
  to(c);

  string out_s;
  google::protobuf::TextFormat::PrintToString(c, &out_s);
  ofs << out_s;

  update_config_write_time();

  if (!ofs) {
    LOG(llvl::error) << "Failed to write config to: " << file;
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

void fc::Controller::shutdown_threads(vector<thread> &threads) {
  for (auto &t : threads) { // Interrupt all threads still running
    t.interrupt();
  }

  for (auto &t : threads) { // Wait for all threads to shutdown
    if (t.joinable())
      t.join();
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

bool fc::Controller::tests_running(
    const vector<shared_ptr<double>> &test_completion) {
  return std::any_of(test_completion.begin(), test_completion.end(),
                     [](const auto c) { return *c < 1; });
}

double fc::Controller::sum(const vector<shared_ptr<double>> &numbers) {
  return std::accumulate(
      numbers.begin(), numbers.end(), 0.0,
      [](const double &total, const auto &b_ptr) { return total + *b_ptr; });
}
