#include "SensorController.hpp"

fancon::SensorController::SensorController(bool debug) {
  sensors_init(NULL);
  sensor_chips = getSensorChips();

  fancon::Util::openSyslog(debug);
}

fancon::SensorController::~SensorController() {
  sensors_cleanup();
  fancon::Util::closeSyslog();
}

vector<SensorChip> fancon::SensorController::getSensorChips() {
  vector<SensorChip> sensor_chips;
  const sensors_chip_name *cn = nullptr;
  int ci = 0;

  while ((cn = sensors_get_detected_chips(NULL, &ci)) != NULL)
    sensor_chips.push_back(std::make_shared<const sensors_chip_name *>(cn));

  return sensor_chips;
}

vector<UID> fancon::SensorController::getUIDs(const char *dev_pf) {
  vector<UID> uids;
  const sensors_feature *sf;

  for (auto &sc : sensor_chips) {
    const sensors_chip_name *cn = *sc;
    int hwmon_id = getLastNum(string(cn->path));

    int sfi = 0;
    while ((sf = sensors_get_features(cn, &sfi)) != NULL) {
      // check for supported device postfix, e.g. fan or temp
      string name(sf->name);
      if (name.find(dev_pf) != string::npos)
        uids.push_back(fancon::UID(cn->prefix, hwmon_id, name));
    }
  }

  return uids;
}

void fancon::SensorController::writeConf(const string &path) {
  // TODO: set up fans as running - i.e. inc default temp_uid and set current temp & rpm for fanconfig
  auto f_uids = getUIDs(Fan::path_pf);
  std::fstream fs(path, std::ios_base::in);   // read from the beginning of the file, append writes
  bool pExists = exists(path);
  bool sccFound = false;

  vector<vector<UID>::iterator> curUIDIts;
  if (pExists) { // read existing UIDs
    for (string line; std::getline(fs, line, '\n');) {
      if (line.empty())
        continue;

      istringstream liss(line);
      /*if (!sccFound) {
        if (SensorControllerConfig(liss).valid()) {
          sccFound = true;
          continue;
        } else
          liss.seekg(0, std::ios::beg);
      }*/

      UID fanUID(liss);

      vector<UID>::iterator it;
      if (fanUID.valid() && (it = find(f_uids.begin(), f_uids.end(), fanUID)) != f_uids.end())
        curUIDIts.push_back(it);
    }
    fs.close();
  }

  fs.open(path, std::ios_base::app);  // out

  auto writeTop = [](std::fstream &fs) {
    fs << "# update: time between rpm changes (in seconds); dynamic: interpolated rpm between two points "
       << "(based on temperature), or static to the next highest point (false)" << endl
       << SensorControllerConfig() << endl
       << "# Example:      15°C stopped fan, 25°C 500 RPM ... 80°C full speed fan" << endl
       << "# it8728/2:fan1 coretemp/0:temp2   [15;0] [25:500] [35:650] [55:1000] [65:1200] [80;255]" << endl
       << "#" << endl
       << "# <Fan UID>     <Temperature Sensor UID>    <[temperature (°C): speed (RPM); PWM (0-255)]>" << endl;
  };

  if (!pExists)
    writeTop(fs);

  // TODO: write SCC to front of file
  /*if (!sccFound) {
    // copy file contents to memory
    fs.open(path, std::ios::in | std::ios::ate);
    fs.seekg(0, std::ios::beg);
    std::stringstream ss;
    ss << fs.rdbuf();
//    vector<char> buf(size);
//    fs.read(buf.data(), size);
    fs.close();

    // write file with scc included
    fs.open(path, std::ios::out);
    writeTop(fs);
//    fs << ss.rdbuf();
  } */

  // write UIDs (not already in file)
  for (auto it = f_uids.begin(); it != f_uids.end(); ++it)
    if (find(curUIDIts.begin(), curUIDIts.end(), it) == curUIDIts.end())    // not currently configured
      fs << *it << endl;

  if (fs.fail())
    log(LOG_ERR, "Failed to write config file: " + path);
}

vector<fancon::TSParent> fancon::SensorController::readConf(const string &path) {
  ifstream ifs(path);
  vector<fancon::TSParent> ts_parents;

  bool sccFound = false;

  // Fan_UID TS_UID FanConfig
  for (string line; std::getline(ifs, line, '\n');) {
    if (line.empty() || line.front() == '#')  // skip line if empty or prefaced with '#'
      continue;

    istringstream liss(line);

    // read and set SensorControllerConfig (if valid)
    if (!sccFound) {
      fancon::SensorControllerConfig fcc(liss);

      if (fcc.valid()) {
        conf = fcc;
        sccFound = true;
        continue;
      } else
        liss.seekg(0, std::ios::beg);
    }

    UID fan_uid(liss);
    UID ts_uid(liss);
    Config fan_conf(liss);

    // check conf line is valid
    if (!fan_uid.valid() || !ts_uid.valid() || !fan_conf.valid())
      continue;

    Fan f(fan_uid, fan_conf, conf.dynamic);

    // TODO: test fan and add PWM values where there are RPMs <<
//        for (auto &p : fan_conf.points)
//            if (!p.validPWM())
//                p.pwm = f.testPWM(p.rpm);

    auto tsp_it = find_if(ts_parents.begin(), ts_parents.end(),
                          [&ts_uid](const fancon::TSParent &p) { return p.ts_uid == ts_uid; });

    if (tsp_it != ts_parents.end())     // found, push fan onto stack
      tsp_it->fans.push_back(f);
    else // make new TSParent
      ts_parents.push_back(TSParent(ts_uid, f));
  }

  return ts_parents;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void fancon::SensorController::run(vector<TSParent>::iterator first, vector<TSParent>::iterator last,
                                   DaemonState &state) const {
  while (state == fancon::Util::DaemonState::RUN) {
    for (auto tsp_it = first; tsp_it != last; ++tsp_it) {
      // TODO: utilize step_ut & step_dt

      // update fans if temp changed
      if (tsp_it->update())
        for (auto &f : tsp_it->fans)
          f.update(tsp_it->temp);
    }

    sleep(conf.update_time_s);
  }
}
#pragma clang diagnostic pop

bool fancon::TSParent::update() {
  int newTemp = fancon::TemperatureSensor::getTemp(ts_uid.getBasePath());

  if (newTemp != temp) {
    temp = newTemp;
    return true;
  } else
    return false;
}