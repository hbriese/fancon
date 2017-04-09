#include <shared_mutex>
#include "Controller.hpp"

using namespace fancon;

namespace fancon {
ControllerState controller_state;
}

/// \param configPath Path to a file containing the ControllerConfig and user fan configurations
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

    UID fanUID(liss);
    UID sensorUID(liss);
    fan::Config fanConf(liss);

    // UIDs, and config must be valid
    if (!fanUID.valid(DeviceType::fan_interface) || !sensorUID.valid(DeviceType::sensor_interface) || !fanConf.valid())
      continue;

    // TODO: test fan and add PWM values where there are RPMs <<
//        for (auto &p : fanConf.points)
//            if (!p.validPWM())
//                p.pwm = f.testPWM(p.rpm);

    // Find sensors if it has already been defined
    auto sIt = find_if(sensors.begin(), sensors.end(),
                       [&](const unique_ptr<SensorInterface> &s) { return *s == sensorUID; });

    // Add the sensor if it is missing, and update the sensor iterator
    if (sIt == sensors.end()) {
      sensors.emplace_back(Find::getSensor(sensorUID));
      sIt = prev(sensors.end());
    }

    // Add fan only if there are configured points
    auto fan = Find::getFan(fanUID, fanConf, conf.dynamic);
    if (!fan->points.empty())
      fans.emplace_back(MappedFan(move(fan), *(*sIt)));
    else
      LOG(llvl::warning) << fanUID << " has no valid points";
  }

  // Shrink containers
  sensors.shrink_to_fit();
  fans.shrink_to_fit();
}

/// \return Controller state
ControllerState Controller::run() {
  if (fans.empty()) {
    LOG(llvl::info) << "No fan configurations found, exiting fancond. See 'fancon -help'";
    return ControllerState::stop;
  }

  // Handle process signals
  struct sigaction act;
  act.sa_handler = &Controller::signalHandler;
  sigemptyset(&act.sa_mask);
  for (auto &s : {SIGTERM, SIGINT, SIGABRT, SIGHUP})
    sigaction(s, &act, nullptr);

  // Start sensor & fan threads defered - to avoid data races
  controller_state = ControllerState::defered_start;

  auto sensorThreadTasks2 = Util::distributeTasks(conf.max_threads, sensors);
  for (auto &tasks : sensorThreadTasks2)
    threads.emplace_back(thread([this, &tasks] {
      deferStart(sensors_wakeup);
      readSensors(tasks);
    }));

  auto fanThreadTasks2 = Util::distributeTasks(conf.max_threads, fans);
  for (auto &tasks : fanThreadTasks2)
    threads.emplace_back(thread([this, &tasks] {
      deferStart(fans_wakeup);
      updateFans(tasks);
    }));

  threads.shrink_to_fit();

  LOG(llvl::debug) << "Started with " << threads.size() << " threads";

  // Synchronize thread wake-up times
  startThreads();
  while (controller_state == ControllerState::run) {
    updateWakeupTimes();
    sleep_until(main_wakeup);
  }

  for (auto &t : threads)
    if (t.joinable())
      t.join();
    else
      LOG(llvl::debug) << "Unable to join thread ID: " << t.get_id();

  return controller_state;
}

/// \copydoc Controller::Controller(const string &configPath)
void Controller::reload(const string &configPath) {
  *this = Controller(configPath);
}

void Controller::signalHandler(int sig) {
  switch (sig) {
  case SIGTERM:
  case SIGINT:
  case SIGABRT: controller_state = ControllerState::stop;
    break;
  case SIGHUP: controller_state = ControllerState::reload;
    break;
  default: LOG(llvl::warning) << "Unknown signal caught (" << sig << "): " << strsignal(sig);
  }
}

/// \return True if the line doesn't start with '#' and isn't just spaces/tabs
bool Controller::validConfigLine(const string &line) {
  // Skip spaces, tabs & whitespace
  auto beg = find_if(line.begin(), line.end(), [](const char &c) { return !std::isspace(c); });

  if (beg == line.end())
    return false;
  else
    return *beg != '#';
}

void Controller::readSensors(vector<sensor_container_t::iterator> &sensors) {
  while (controller_state == ControllerState::run) {
    for (auto &it : sensors)
      (*it)->refresh();

    sleep_until(sensors_wakeup);
  }
}

void Controller::updateFans(vector<fan_container_t::iterator> &fans) {
  while (controller_state == ControllerState::run) {
    // Update fan speed if sensor's temperature has changed
    for (auto &it : fans)
      if (it->sensor.update)
        it->fan->update(it->sensor.temp);

    sleep_until(fans_wakeup);
  }
}

void Controller::startThreads() {
  main_wakeup = chrono::time_point_cast<milliseconds>(chrono::steady_clock::now());
  controller_state = ControllerState::run;
  updateWakeupTimes();
}

void Controller::updateWakeupTimes() {
  main_wakeup += conf.update_interval;
  sensors_wakeup = main_wakeup + milliseconds(20);
  fans_wakeup = sensors_wakeup + milliseconds(100);
}

/// \brief Lock while start is defered, then sleep until the given time point
void Controller::deferStart(chrono::time_point <chrono::steady_clock, milliseconds> &timePoint) {
  while (controller_state == ControllerState::defered_start)
    sleep_for(milliseconds(1));

  sleep_until(timePoint);
}