#include "Controller.hpp"

using namespace fancon;

Controller::Controller(uint nThreads) {
#ifdef FANCON_NVIDIA_SUPPORT
  nvidia_control = NV::supported();
//  LOG(llvl::debug) << "Nvidia control " << ((!nvidia_control) ? "un" : "") << "supported";
//  if (nvidia_control)
//    NV::enableFanControlCoolbit();
#endif //FANCON_NVIDIA_SUPPORT

  if (nThreads)
    conf.threads = nThreads;
}

vector<UID> Controller::getFans() {
  constexpr const char *fanPathPostfix = "fan";
  auto fans = getUIDs(fanPathPostfix);

#ifdef FANCON_NVIDIA_SUPPORT
  auto nvFans = NV::getFans(nvidia_control);
  Util::moveAppend(nvFans, fans);
#endif //FANCON_NVIDIA_SUPPORT

  return fans;
}

vector<UID> Controller::getSensors() {
  auto sensors = getUIDs(Util::temp_sensor_label);

#ifdef FANCON_NVIDIA_SUPPORT
  auto nvSensors = NV::getSensors(nvidia_control);
  Util::moveAppend(nvSensors, sensors);
#endif //FANCON_NVIDIA_SUPPORT

  return sensors;
}

unique_ptr<SensorInterface> Controller::getSensor(const UID &uid) {
  assert(uid.isSensor());

  if (uid.type == DeviceType::SENSOR)
    return make_unique<Sensor>(uid);
  else
    return make_unique<SensorNV>(uid);
}

unique_ptr<FanInterface> Controller::getFan(const UID &uid, const FanConfig &fanConf, bool dynamic) {
  assert(uid.isFan());

  if (uid.type == DeviceType::FAN)
    return make_unique<Fan>(uid, fanConf, dynamic);
  else
    return make_unique<FanNV>(uid, fanConf, dynamic);
}

void Controller::writeConf(const string &path) {
  // TODO: set up fans as running - i.e. inc default temp_uid and set current temp & rpm for fanconfig
  auto fUIDs = getFans();
  std::fstream fs(path, std::ios_base::in);   // read from the beginning of the file, append writes
  bool pExists = exists(path);
  bool controllerConfigFound = false;

  vector<vector<UID>::iterator> curUIDIts;
  if (pExists) { // read existing UIDs
    LOG(llvl::info) << "Config exists, adding absent fans";

    for (string line; std::getline(fs, line);) {
      if (skipLine(line))
        continue;

      istringstream liss(line);
      if (!controllerConfigFound) {
        if (ControllerConfig(liss).valid()) {
          controllerConfigFound = true;
          continue;
        } else
          liss.seekg(0, std::ios::beg);
      }

      UID fanUID(liss);

      // Add iterator to list if already added
      vector<UID>::iterator it;
      if (fanUID.valid(DeviceType::FAN_INTERFACE) && (it = find(fUIDs.begin(), fUIDs.end(), fanUID)) != fUIDs.end())
        curUIDIts.push_back(it);
    }
    fs.close();
  }

  fs.open(path, std::ios_base::app);  // out

  auto writeTop = [](std::fstream &s) {
    s << "# refresh: time between rpm changes (in seconds); threads: max number of threads to run fancond\n"
      << "# dynamic: interpolated rpm between two points (based on temperature) rather than next point\n"
      << ControllerConfig() << "\n\n"
      << "# Note:\n"
      << "# fancon test MUST be run (on the fan) before use of RPM for speed control\n"
      << "# Append 'f' to the temperature for fahrenheit e.g. [86f:500]\n\n"
      << "# Example:      Below 30°C fan stopped, 86°F (30°C) 500 RPM ... 90°C full speed\n"
      << "# it8728/2:fan1 coretemp/0:temp2   [0;0] [86f:500] [45:650] [55:1000] [65:1200] [90;255]\n"
      << "#\n"
      << "# <Fan UID>     <Sensor UID>    <[temperature (°C): speed (RPM); PWM (0-255)]>";
  };

  if (!pExists) {
    LOG(llvl::info) << "Writing new config: " << path;
    writeTop(fs);
  }

  // write UIDs (not already in file)
  for (auto it = fUIDs.begin(); it != fUIDs.end(); ++it)
    if (find(curUIDIts.begin(), curUIDIts.end(), it) == curUIDIts.end())    // not currently configured
      fs << *it << '\n';

  if (fs.fail())
    LOG(llvl::error) << "Failed to write config file: " << path;
}

unique_ptr<Daemon> Controller::loadDaemon(const string &configPath, DaemonState &daemonState) {
  auto d = make_unique<Daemon>(daemonState, conf.update_time);

  ifstream ifs(configPath);
  bool sccFound = false;

  /* FORMAT:
  * ControllerConfig
  * Fan_UID TS_UID Config   */
  for (string line; std::getline(ifs, line);) {
    if (skipLine(line))
      continue;

    istringstream liss(line);

    if (!sccFound) {
      // Read line and set ControllerConfig (if valid)
      fancon::ControllerConfig fcc(liss);

      if (fcc.valid()) {
        conf = fcc;
        sccFound = true;
        continue;
      } else
        liss.seekg(0, std::ios::beg);
    }

    UID fanUID(liss);
    UID sensorUID(liss);
    FanConfig fanConf(liss);

    // Validate all UIDs
    if (!fanUID.valid(DeviceType::FAN_INTERFACE) || !sensorUID.valid(DeviceType::SENSOR_INTERFACE) || !fanConf.valid())
      continue;

    // TODO: test fan and add PWM values where there are RPMs <<
//        for (auto &p : fanConf.points)
//            if (!p.validPWM())
//                p.pwm = f.testPWM(p.rpm);

    // Find sensors if it has already been defined
    auto sIt = find_if(d->sensors.begin(), d->sensors.end(),
                       [&sensorUID](const unique_ptr<SensorInterface> &s) { return *s == sensorUID; });

    // Add the sensor if it is missing, and update the sensor iterator
    if (sIt == d->sensors.end()) {
      d->sensors.emplace_back(getSensor(sensorUID));
      sIt = prev(d->sensors.end());
    }

    d->fans.emplace_back(make_unique<MappedFan>(getFan(fanUID, fanConf, conf.dynamic), (*sIt)->temp));
  }

  return d;
}

//vector<unique_ptr<MappedFan>> Controller::readConf(const string &path) {
//  vector<unique_ptr<MappedFan>> mappedFans;   // TODO: consider removal of up from vector<unique_ptr<>>
//  ifstream ifs(path);
//  bool sccFound = false;
//
//  /* FORMAT:
//  * ControllerConfig
//  * Fan_UID TS_UID Config   */
//  for (string line; std::getline(ifs, line);) {
//    if (skipLine(line))
//      continue;
//
//    istringstream liss(line);
//
//    if (!sccFound) {
//      // Read line and set ControllerConfig (if valid)
//      fancon::ControllerConfig fcc(liss);
//
//      if (fcc.valid()) {
//        conf = fcc;
//        sccFound = true;
//        continue;
//      } else
//        liss.seekg(0, std::ios::beg);
//    }
//
//    UID fUID(liss);
//    UID sUID(liss);
//    FanConfig fanConf(liss);
//
//    // Validate all UIDs
//    if (!fUID.valid(DeviceType::FAN_INTERFACE) || !sUID.valid(DeviceType::SENSOR_INTERFACE) || !fanConf.valid())
//      continue;
//
//    // TODO: test fan and add PWM values where there are RPMs <<
////        for (auto &p : fanConf.points)
////            if (!p.validPWM())
////                p.pwm = f.testPWM(p.rpm);
//
//    // Find MappedFan iterator with the same sensor
//    auto sIt = find_if(mappedFans.begin(), mappedFans.end(),
//                       [&sUID](const unique_ptr<MappedFan> &mf) { return *mf->sensor == sUID; });
//
//    // Copy (matching) found sensor shared ptr, or create a new one
//    auto sensor = (sIt != mappedFans.end()) ? (*sIt)->sensor
//                                            : getSensor(sUID);
//
//    mappedFans.emplace_back(make_unique<MappedFan>(move(sensor), getFan(fUID, fanConf, conf.dynamic), sensor.unique()));
//  }
//
//  return mappedFans;
//}

vector<UID> Controller::getUIDs(const char *devPf) {
  vector<UID> uids;
  SensorsWrapper sensors;

  for (const auto &sc : sensors.chips) {
    int hwmon_id = getLastNum(string(sc->path));

    int sfi = 0;
    const sensors_feature *sf;
    while ((sf = sensors_get_features(sc, &sfi)) != nullptr) {
      // check for supported device postfix, e.g. fan or temp
      string name(sf->name);
      if (name.find(devPf) != string::npos)
        uids.emplace_back(fancon::UID(sc->prefix, hwmon_id, name));
    }
  }

  return uids;
}

bool Controller::skipLine(const string &line) {
  // break when valid char found
  auto it = line.begin();
  for (; it != line.end(); ++it)
    if (!std::isspace(*it) || *it != '\t')
      break;

  // true if line starts with '#', or all space/tabs
  return line.front() == '#' || it == line.end();
}

SensorsWrapper::SensorsWrapper() {
  sensors_init(nullptr);

  const sensors_chip_name *cn = nullptr;
  int ci = 0;
  while ((cn = sensors_get_detected_chips(nullptr, &ci)) != nullptr)
    chips.push_back(cn);
}

void Daemon::readSensors(vector<unique_ptr<SensorInterface>>::iterator &beg,
                         vector<unique_ptr<SensorInterface>>::iterator &end) {
  while (state == DaemonState::RUN) {
    for (auto &it = beg; it != end; ++it)
      (*it)->refresh();

    sleep_for(update_time);
  }
}

void Daemon::updateFans(vector<unique_ptr<MappedFan>>::iterator &beg,
                        vector<unique_ptr<MappedFan>>::iterator &end) {
  while (state == DaemonState::RUN) {
    for (auto &it = beg; it != end; ++it)
      (*it)->fan->update((*it)->temp);

    sleep_for(update_time);
  }
}