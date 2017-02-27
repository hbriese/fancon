#include "SensorController.hpp"

using namespace fancon;

SensorController::SensorController(bool debug, uint nThreads) {
  sensors_init(NULL);
  sensor_chips = getSensorChips();

  if (checkNvidiaSupport())
    enableNvidiaFanControlCoolbit();

  if (nThreads)
    conf.threads = nThreads;

  fancon::Util::openSyslog(debug);
}

SensorController::~SensorController() {
  sensors_cleanup();
  fancon::Util::closeSyslog();
}

vector<SensorChip> SensorController::getSensorChips() {
  vector<SensorChip> sensor_chips;
  const sensors_chip_name *cn = nullptr;
  int ci = 0;

  while ((cn = sensors_get_detected_chips(NULL, &ci)) != NULL)
    sensor_chips.push_back(make_unique<const sensors_chip_name *>(cn));

  return sensor_chips;
}

bool SensorController::skipLine(const string &line) {
  // break when valid char found
  auto it = line.begin();
  for (; it != line.end(); ++it)
    if (!std::isspace(*it) || *it != '\t')
      break;

  // true if line starts with '#', or all space/tabs
  return line.front() == '#' || it == line.end();
}

vector<UID> SensorController::getNvidiaFans() {
  /* Sample output:
   * [0] percy-dh:0[gpu:0] (GeForce GTX 980 Ti) */

  const char idBegSep = '[', idEndSep = ']';  // first occurrence of these ONLY
  const char modelBegSep = '(', modelEndSep = ')';

  vector<UID> uids;
  redi::ipstream ips("nvidia-settings -q gpus | grep gpu:");

  string line;
  while (std::getline(ips, line)) {
    auto idBegIt = find(line.begin(), line.end(), idBegSep) + 1;
    auto idEndIt = find(idBegIt, line.end(), idEndSep);

    auto modelBegIt = find(idEndIt, line.end(), modelBegSep);
    auto modelEndIt = find(modelBegIt, line.end(), modelEndSep);
    auto nameBegIt = find_if(modelBegIt, modelEndIt, [](const char &c) { return std::isdigit(c); });
    auto &nameEndIt = modelEndIt;

    string idStr(idBegIt, idEndIt);
    if (validIter(line.end(), {idBegIt, idEndIt, nameBegIt, nameEndIt}) && Util::isNum(idStr)) {
      string name(nameBegIt, nameEndIt);
      std::replace(name.begin(), name.end(), ' ', '_');   // replace spaces in name with underscores
      uids.emplace_back(fancon::UID(string(fancon::Util::nvidia_label), std::stoi(idStr), name));
    } else
      log(LOG_DEBUG, string("Invalid nvidia grep line: ") + line);
  }

  // enable NVIDIA fan control coolbits bit if required
  enableNvidiaFanControlCoolbit();

  return uids;
}

vector<UID> SensorController::getFansAll() {
  auto fans = getFans();
  auto nvFans = getNvidiaFans();
  Util::moveAppend(nvFans, fans);

  return fans;
}

void SensorController::writeConf(const string &path) {
  // TODO: set up fans as running - i.e. inc default temp_uid and set current temp & rpm for fanconfig
  auto fUIDs = getFansAll();
  std::fstream fs(path, std::ios_base::in);   // read from the beginning of the file, append writes
  bool pExists = exists(path);
  bool sccFound = false;

  vector<vector<UID>::iterator> curUIDIts;
  if (pExists) { // read existing UIDs
    log(LOG_NOTICE, "Config exists, adding absent fans");

    for (string line; std::getline(fs, line);) {
      if (skipLine(line))
        continue;

      istringstream liss(line);
      if (!sccFound) {
        if (SensorControllerConfig(liss).valid()) {
          sccFound = true;
          continue;
        } else
          liss.seekg(0, std::ios::beg);
      }

      UID fanUID(liss);

      vector<UID>::iterator it;
      if (fanUID.valid() && (it = find(fUIDs.begin(), fUIDs.end(), fanUID)) != fUIDs.end())
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
       << "# it8728/2:fan1 coretemp/0:temp2   [0;0] [77f:500] [35:650] [55:1000] [65:1200] [80;255]" << endl
       << "#" << endl
       << "# <Fan UID>     <Temperature Sensor UID>    <[temperature (°C): speed (RPM); PWM (0-255)]>" << endl;
  };

  if (!pExists) {
    log(LOG_NOTICE, "Writing new config: " + path);
    writeTop(fs);
  }

  // write UIDs (not already in file)
  for (auto it = fUIDs.begin(); it != fUIDs.end(); ++it)
    if (find(curUIDIts.begin(), curUIDIts.end(), it) == curUIDIts.end())    // not currently configured
      fs << *it << endl;

  if (fs.fail())
    log(LOG_ERR, "Failed to write config file: " + path);
}

vector<unique_ptr<TSParent>> SensorController::readConf(const string &path) {
  ifstream ifs(path);
  vector<unique_ptr<TSParent>> tsParents;

  bool sccFound = false;

  /* FORMAT:
  * SensorControllerConfig
  * Fan_UID TS_UID Config   */
  for (string line; std::getline(ifs, line);) {
    if (skipLine(line))
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

    UID fanUID(liss);
    UID tsUID(liss);
    FanConfig fanConf(liss);

    // check conf line is valid
    if (!fanUID.valid() || !tsUID.valid() || !fanConf.valid())
      continue;

    // TODO: test fan and add PWM values where there are RPMs <<
//        for (auto &p : fanConf.points)
//            if (!p.validPWM())
//                p.pwm = f.testPWM(p.rpm);

    auto tspIt = find_if(tsParents.begin(), tsParents.end(),
                         [&tsUID](const unique_ptr<TSParent> &pp) -> bool { return pp->ts_uid == tsUID; });

    if (tspIt == tsParents.end()) {   // make new TSP if missing
      tsParents.emplace_back(make_unique<TSParent>(tsUID));
      tspIt = tsParents.end() - 1;
    }

    if (fanUID.type == FAN_NVIDIA) {
      auto fn = make_unique<FanNVIDIA>(fanUID, fanConf, conf.dynamic);
      if (!fn->points.empty())
        (*tspIt)->fansNVIDIA.emplace_back(move(fn));
      else
        fn.reset();
    } else {
      auto f = make_unique<Fan>(fanUID, fanConf, conf.dynamic);
      if (!f->points.empty())
        (*tspIt)->fans.emplace_back(move(f));
      else
        f.reset();
    }
  }

  return tsParents;
}

vector<UID> SensorController::getUIDs(const char *devPf) {
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
        uids.emplace_back(fancon::UID(cn->prefix, hwmon_id, name));
    }
  }

  return uids;
}

bool SensorController::checkNvidiaSupport() {
  string c("which nvidia-settings");
  redi::ipstream ips(c);
  string l;
  std::getline(ips, l);

  return !l.empty();
}

void SensorController::enableNvidiaFanControlCoolbit() {
  string command("sudo nvidia-xconfig -t | grep Coolbits");
  redi::pstream ips(command);
  string l;
  std::getline(ips, l);
  int iv = (!l.empty()) ? Util::getLastNum(l) : 0;  // initial value
  int cv(iv);   // current

  vector<int> cbV{1, 2, 4, 8, 16};
  int fanControlBit = 2;
  vector<bool> cbS(cbV.size(), 0);

  for (auto i = cbV.size() - 1; i > 0 && cv > 0; --i)
    if ((cbS[i] = (cv / cbV[i]) >= 1))
      cv -= cbV[i];

  int nv = iv;
  if (cv > 0) {
    log(LOG_ERR, "Invalid coolbits value, fixing with fan control bit set");
    nv -= cv;
  }

  if (!cbS[fanControlBit])
    nv += cbV[fanControlBit];

  if (nv != iv) {
    command = string("sudo nvidia-xconfig --cool-bits=") + to_string(nv) + " > /dev/null";
    system(command.c_str());
    log(LOG_NOTICE, "Reboot, or restart your display server to enable NVIDIA fan control");
  }
}

TSParent::TSParent(TSParent &&other) : ts_uid(other.ts_uid), temp(other.temp) {
  for (auto &fp : fans)
    fans.emplace_back(move(fp));
  fans.clear();

  for (auto &fp : fansNVIDIA)
    fansNVIDIA.emplace_back(move(fp));
  fansNVIDIA.clear();
}

bool TSParent::update() {
  int newTemp = TemperatureSensor::getTemp(ts_uid.getBasePath());

  if (newTemp != temp) {
    temp = newTemp;
    return true;
  }
  return false;
}