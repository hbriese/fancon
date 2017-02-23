#include "SensorController.hpp"

fancon::SensorController::SensorController(bool debug, uint nThreads) {
  sensors_init(NULL);
  sensor_chips = getSensorChips();

  if (nThreads)
    conf.threads = nThreads;

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

vector<UID> fancon::SensorController::getUIDs(const char *devPf) {
  vector<UID> uids;
  const sensors_feature *sf;

  for (auto &sc : sensor_chips) {
    const sensors_chip_name *cn = *sc;
    int hwmon_id = getLastNum(string(cn->path));

    int sfi = 0;
    while ((sf = sensors_get_features(cn, &sfi)) != NULL) {
      // check for supported device postfix, e.g. fan or temp
      string name(sf->name);
      if (name.find(devPf) != string::npos)
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
//  bool sccFound = false;

  vector<vector<UID>::iterator> curUIDIts;
  if (pExists) { // read existing UIDs
    log(LOG_NOTICE, "Config exists, adding absent fans");

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
    fs << "# update: time between rpm changes (in seconds); threads: max number of threads to run fancond" << endl
       << "# dynamic: interpolated rpm between two points (based on temperature) rather than next point" << endl
       << SensorControllerConfig() << endl << endl
       << "# Note:" << endl
       << "# fancon test MUST be run (on the fan) before use of RPM for speed control" << endl
       << "# Append 'f' to the temperature for fahrenheit" << endl << endl
       << "# Example:      Below 22°C fan stopped, 77°F (22°C) 500 RPM ... 80°C full speed fan" << endl
       << "# it8728/2:fan1 coretemp/0:temp2   [15;0] [77f:500] [35:650] [55:1000] [65:1200] [80;255]" << endl
       << "#" << endl
       << "# <Fan UID>     <Temperature Sensor UID>    <[temperature (°C): speed (RPM); PWM (0-255)]>" << endl;
  };

  if (!pExists) {
    log(LOG_NOTICE, "Writing new config: " + path);
    writeTop(fs);
  }

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

vector<unique_ptr<fancon::TSParent>> fancon::SensorController::readConf(const string &path) {
  ifstream ifs(path);
  vector<unique_ptr<fancon::TSParent>> tsParents;

  bool sccFound = false;

  /* FORMAT:
  * SensorControllerConfig
  * Fan_UID TS_UID Config   */
  for (string line; std::getline(ifs, line);) {
    // remove prefacing ' ' or '/t's
    bool stillSkip = true;
    std::remove_if(line.begin(), line.end(), [&stillSkip](const char &c) -> bool {
      return stillSkip && (stillSkip = (std::isspace(c) | (c == '\t')));
    });

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
    FanConfig fan_conf(liss);

    // check conf line is valid
    if (!fan_uid.valid() || !ts_uid.valid() || !fan_conf.valid())
      continue;

    // TODO: test fan and add PWM values where there are RPMs <<
//        for (auto &p : fan_conf.points)
//            if (!p.validPWM())
//                p.pwm = f.testPWM(p.rpm);

    auto tspIt = find_if(tsParents.begin(), tsParents.end(),
                         [&ts_uid](const unique_ptr<TSParent> &pp) -> bool { return pp->ts_uid == ts_uid; });

    auto f = make_unique<Fan>(fan_uid, fan_conf, conf.dynamic);

    if (tspIt != tsParents.end())       // found, push fan onto stack
      (*tspIt)->fans.emplace_back(move(f));
    else                                // make new TSParent
      tsParents.emplace_back(make_unique<TSParent>(ts_uid, move(f)));
  }

//  return ts_parents;
  return tsParents;
}

fancon::TSParent::TSParent(UID tsUID, unique_ptr<Fan> fp, int temp)
    : ts_uid(tsUID), temp(temp) { fans.emplace_back(move(fp)); }

fancon::TSParent::TSParent(TSParent &&other) : ts_uid(other.ts_uid), temp(other.temp) {
  for (auto &fp : fans)
    fans.emplace_back(move(fp));
  fans.clear();
}

bool fancon::TSParent::update() {
  int newTemp = fancon::TemperatureSensor::getTemp(ts_uid.getBasePath());

  if (newTemp != temp) {
    temp = newTemp;
    return true;
  }
  return false;
}