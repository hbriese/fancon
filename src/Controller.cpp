#include "Controller.hpp"

using namespace fancon;

namespace fancon {
ControllerState controller_state{ControllerState::stop};
}

Controller::Controller(const string &configPath) {
  ifstream ifs(configPath);
  bool configFound = false;

  /* FORMAT: note. lines unordered
  * Config
  * Fan_UID TS_UID Config   */
  for (string line; std::getline(ifs >> std::skipws, line);) {
    if (!validConfigLine(line))
      continue;

    istringstream liss(line);

    // Check for Config (if not found already
    if (!configFound) {
      // Read line and set Config (if valid)
      controller::Config fcc(liss);

      if (fcc.valid()) {
        conf = move(fcc);
        configFound = true;
        continue;
      } else
        liss.seekg(0, std::ios::beg);
    }

    // Deserialize, then check input is valid
    UID fanUID(liss);
    if (!fanUID.valid(DeviceType::fan_interface)
#ifdef FANCON_NVIDIA_SUPPORT
        ||
            ((fanUID.type == DeviceType::fan_nv) & !NV::support)
#endif
        )
      continue;

    UID sensorUID(liss);
    if (!sensorUID.valid(DeviceType::sensor_interface))
      continue;

    fan::Config fanConf(liss);
    if (!fanConf.valid())
      continue;

    // Find iterator of existing sensor
    auto sIt = find_if(
        sensors.begin(), sensors.end(),
        [&](const unique_ptr<SensorInterface> &s) { return *s == sensorUID; });

    // Add sensor if iterator not found, then update iterator
    if (sIt == sensors.end())
      sIt = sensors.emplace(sensors.end(), Devices::getSensor(sensorUID));

    // Add fan only if there are configured points
    auto fan = Devices::getFan(fanUID, fanConf, conf.dynamic);
    if (!fan->points.empty())
      fans.emplace_back(MappedFan(move(fan), *(*sIt)));
    else
      LOG(llvl::warning) << fanUID << " has no valid configuration";
  }

  // Shrink containers
  sensors.shrink_to_fit();
  fans.shrink_to_fit();
}

/// \return Controller state
ControllerState Controller::run() {
  if (fans.empty()) {
    LOG(llvl::info) << "No fan configurations found, exiting fancon daemon. "
          "See 'fancon -help'";
    return ControllerState::stop;
  }

  // Handle process signals
  struct sigaction act;
  act.sa_handler = &Controller::signalHandler;
  sigemptyset(&act.sa_mask);
  for (auto &s : {SIGTERM, SIGINT, SIGABRT, SIGHUP})
    sigaction(s, &act, nullptr);

  // Run sensor & fan threads
  vector<thread> threads;
  {
    auto sensorTasks = Util::distributeTasks(conf.max_threads, sensors);
    auto fanTasks = Util::distributeTasks(conf.max_threads, fans);
    threads.reserve(sensorTasks.size() + fanTasks.size());

    for (auto &t : sensorTasks)
      threads.emplace_back(thread(&Controller::updateSensors, this, move(t)));

    for (auto &t : fanTasks)
      threads.emplace_back(thread(&Controller::updateFans, this, move(t)));
  }
  threads.shrink_to_fit();

  LOG(llvl::debug) << "Started with " << (threads.size() + 1) << " threads";

  // Synchronize thread wake-up times in main thread
  scheduler();

  for (auto &t : threads)
    if (t.joinable())
      t.join();

  return controller_state;
}

void Controller::updateSensors(vector<sensor_container_t::iterator> sensors) {
  deferStart(sensors_wakeup);

  while (controller_state == ControllerState::run) {
    for (auto &it : sensors)
      (*it)->refresh();

    sleep_until(sensors_wakeup);
  }
}

void Controller::updateFans(vector<fan_container_t::iterator> fans) {
  deferStart(fans_wakeup);

  while (controller_state == ControllerState::run) {
    // Update fan speed if sensor's temperature has changed
    for (auto &it : fans)
      if (it->sensor.update)
        it->fan->update(it->sensor.temp);

    sleep_until(fans_wakeup);
  }
}

/// \warning Fans may update before sensor if sensor update takes >100ms
void Controller::scheduler() {
  auto schedulerWakeup =
      chrono::time_point_cast<milliseconds>(chrono::steady_clock::now());

  auto update = [this, &schedulerWakeup](const milliseconds &interval) {
    schedulerWakeup += interval;
    sensors_wakeup = schedulerWakeup + milliseconds(20);
    fans_wakeup = sensors_wakeup + milliseconds(100);
  };

  // Wake deferred threads almost immediately
  update(milliseconds(1));
  controller_state = ControllerState::run;
  sleep_until(schedulerWakeup);

  // Wake threads once every update_interval
  while (controller_state == ControllerState::run) {
    update(conf.update_interval);
    sleep_until(schedulerWakeup);
  }
}

/// \brief Sets controller_state if appropriate signals are recieved
void Controller::signalHandler(int sig) {
  switch (sig) {
  case SIGTERM:
  case SIGINT:
  case SIGABRT:controller_state = ControllerState::stop;
    break;
  case SIGHUP:controller_state = ControllerState::reload;
    break;
  default:LOG(llvl::warning) << "Unhandled signal caught: " << sig << " - "
                             << strsignal(sig);
  }
}

/// \return True if the line doesn't start with '#' and isn't just spaces/tabs
bool Controller::validConfigLine(const string &line) {
  // Skip spaces, tabs & whitespace
  auto beg = std::find_if_not(line.begin(), line.end(),
                              [](const char &c) { return std::isspace(c); });

  if (beg == line.end())
    return false;

  return *beg != '#';
}

/// \brief Lock until ControllerState::run, then sleep until timePoint
void Controller::deferStart(time_point_t &timePoint) {
  while (controller_state != ControllerState::run)
    sleep_for(milliseconds(10));

  sleep_until(timePoint);
}
