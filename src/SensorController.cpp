#include "SensorController.hpp"

using namespace fancon;

SensorController::SensorController(uint nThreads) {
#ifdef FANCON_NVIDIA_SUPPORT
  nvidia_control = nvidiaSupported();
//  if (nvidia_control)
//    enableNvidiaFanControlCoolbit();
#endif //FANCON_NVIDIA_SUPPORT

  if (nThreads)
    conf.threads = nThreads;
}

vector<UID> SensorController::getFans() {
  auto fans = getUIDs(Fan::path_pf);

#ifdef FANCON_NVIDIA_SUPPORT
  auto nvFans = getFansNV();
  Util::moveAppend(nvFans, fans);
#endif //FANCON_NVIDIA_SUPPORT

  return fans;
}

vector<UID> SensorController::getSensors() {
  auto sensors = getUIDs(Util::temp_sensor_label);

#ifdef FANCON_NVIDIA_SUPPORT
  auto nvSensors = getSensorsNV();
  Util::moveAppend(nvSensors, sensors);
#endif //FANCON_NVIDIA_SUPPORT

  return sensors;
}

void SensorController::writeConf(const string &path) {
  // TODO: set up fans as running - i.e. inc default temp_uid and set current temp & rpm for fanconfig
  auto fUIDs = getFans();
  std::fstream fs(path, std::ios_base::in);   // read from the beginning of the file, append writes
  bool pExists = exists(path);
  bool sccFound = false;

  vector<vector<UID>::iterator> curUIDIts;
  if (pExists) { // read existing UIDs
    LOG(severity_level::info) << "Config exists, adding absent fans";

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

  auto writeTop = [](std::fstream &s) {
    s << "# update: time between rpm changes (in seconds); threads: max number of threads to run fancond" << endl
      << "# dynamic: interpolated rpm between two points (based on temperature) rather than next point" << endl
      << SensorControllerConfig() << endl << endl
      << "# Note:" << endl
      << "# fancon test MUST be run (on the fan) before use of RPM for speed control" << endl
      << "# Append 'f' to the temperature for fahrenheit" << endl << endl
      << "# Example:      Below 22°C fan stopped, 77°F (22°C) 500 RPM ... 80°C full speed fan" << endl
      << "# it8728/2:fan1 coretemp/0:temp2   [0;0] [77f:500] [35:650] [55:1000] [65:1200] [80;255]" << endl
      << "#" << endl
      << "# <Fan UID>     <Sensor UID>    <[temperature (°C): speed (RPM); PWM (0-255)]>" << endl;
  };

  if (!pExists) {
    LOG(severity_level::info) << "Writing new config: " << path;
    writeTop(fs);
  }

  // write UIDs (not already in file)
  for (auto it = fUIDs.begin(); it != fUIDs.end(); ++it)
    if (find(curUIDIts.begin(), curUIDIts.end(), it) == curUIDIts.end())    // not currently configured
      fs << *it << endl;

  if (fs.fail())
    LOG(severity_level::error) << "Failed to write config file: " << path;
}

vector<unique_ptr<SensorParentInterface>> SensorController::readConf(const string &path) {
  ifstream ifs(path);
  vector<unique_ptr<SensorParentInterface>> sParents;

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

    UID fUID(liss);
    UID sUID(liss);
    FanConfig fanConf(liss);

    // check conf line is valid
    if (!fUID.valid() || !sUID.valid() || !fanConf.valid())
      continue;

    // TODO: test fan and add PWM values where there are RPMs <<
//        for (auto &p : fanConf.points)
//            if (!p.validPWM())
//                p.pwm = f.testPWM(p.rpm);

    auto spIt = find_if(sParents.begin(), sParents.end(),
                        [&sUID](const unique_ptr<SensorParentInterface> &pp) -> bool { return (*pp) == sUID; });

    if (spIt == sParents.end()) {   // make new TSP if missing
      if (sUID.type == SENSOR)
        sParents.emplace_back(make_unique<SensorParent>(sUID));
#ifdef FANCON_NVIDIA_SUPPORT
      else if (sUID.type == SENSOR_NVIDIA) {
        if (nvidia_control)
          sParents.emplace_back(make_unique<SensorParentNV>(sUID));
        else {
          LOG(severity_level::warning) << "Config line is invalid (NVIDIA control is not supported): " << line;
          continue;
        }
      }
#endif //FANCON_NVIDIA_SUPPORT
      else {
        LOG(severity_level::warning) << "Config line is invalid (sensor UID is not a sensor): " << line;
        continue;
      }

      spIt = sParents.end() - 1;
    }

    if (fUID.type == FAN) {
      auto f = make_unique<Fan>(fUID, fanConf, conf.dynamic);
      if (!f->points.empty())
        (*spIt)->fans.emplace_back(move(f));
      else
        f.reset();
    }
#ifdef FANCON_NVIDIA_SUPPORT
    else if (fUID.type == FAN_NVIDIA) {
      if (nvidia_control) {
        auto fn = make_unique<FanNV>(fUID, fanConf, conf.dynamic);
        if (!fn->points.empty())
          (*spIt)->fans.emplace_back(move(fn));
        else
          fn.reset();
      } else
        LOG(severity_level::warning) << "Conf line is invalid (NVIDIA control is not supported): " << line;
    }
#endif //FANCON_NVIDIA_SUPPORT
  }

  return sParents;
}

vector<UID> SensorController::getUIDs(const char *devPf) {
  vector<UID> uids;
  SensorsWrapper sensors;

  for (auto &sc : sensors.chips) {
    int hwmon_id = getLastNum(string(sc->path));

    int sfi = 0;
    const sensors_feature *sf;
    while ((sf = sensors_get_features(sc, &sfi)) != NULL) {
      // check for supported device postfix, e.g. fan or temp
      string name(sf->name);
      if (name.find(devPf) != string::npos)
        uids.emplace_back(fancon::UID(sc->prefix, hwmon_id, name));
    }
  }

  return uids;
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

#ifdef FANCON_NVIDIA_SUPPORT
int SensorController::getNVGPUs() {
  int nGPUs = 0;
  if (!nvidia_control)
    return nGPUs;

  // Number of GPUs

  int ret = XNVCTRLQueryTargetCount(*dw, NV_CTRL_TARGET_TYPE_GPU, &nGPUs);
  if (!ret)
    LOG(severity_level::error) << "Failed to query number of NVIDIA GPUs";

  return nGPUs;
}

vector<int> SensorController::nvProcessBinaryData(const unsigned char *data, const int len) {
  vector<int> v;
  for (int i = 0; i < len; ++i) {
    int val = static_cast<int>(*(data + i));
    if (val == 0)       // memory set to 0 when allocated
      break;
    v.push_back(--val); // return an index (start at 0), binary data start at 1
  }

  return v;
}

vector<UID> SensorController::getFansNV() {
  vector<UID> uids;
  auto nGPUs = getNVGPUs();

  for (int i = 0; i < nGPUs; ++i) {
    vector<int> fanIDs, tSensorIDs;

    // GPU fans
    unsigned char *coolers = nullptr;
    int len = 0;    // buffer size allocated
    int ret = XNVCTRLQueryTargetBinaryData(*dw, NV_CTRL_TARGET_TYPE_GPU, i, 0,
                                           NV_CTRL_BINARY_DATA_COOLERS_USED_BY_GPU, &coolers, &len);
    if (!ret || coolers == nullptr) {
      LOG(severity_level::error) << "Failed to query number of fans for GPU: " << i;
      return uids;
    }
    fanIDs = nvProcessBinaryData(coolers, len);

    // GPU product name
    char *product_name = nullptr;
    ret = XNVCTRLQueryTargetStringAttribute(*dw, NV_CTRL_TARGET_TYPE_GPU, i, 0,
                                            NV_CTRL_STRING_PRODUCT_NAME, &product_name);
    if (!ret || product_name == nullptr) {
      LOG(severity_level::error) << "Failed to query gpu: " << i << " product name";
      return uids;
    }
    string productName(product_name);

    // Disregard preceding titles
    vector<string> titles = {"GeForce", "GTX"};
    auto begIt = productName.begin();
    for (const auto &t : titles) {
      auto newBegIt = search(begIt, productName.end(), t.begin(), t.end()) + t.size();
      if (*newBegIt == ' ') // skip spaces
        ++newBegIt;
      if (newBegIt != productName.end())
        begIt = newBegIt;
    }
    std::replace(begIt, productName.end(), ' ', '_');   // replace spaces with underscores

    for (const auto &fi : fanIDs)
      uids.emplace_back(UID(Util::nvidia_label, fi, string(begIt, productName.end())));
  }

  return uids;
}

vector<UID> SensorController::getSensorsNV() {
  vector<UID> uids;
  auto nGPUs = getNVGPUs();

  for (auto i = 0; i < nGPUs; ++i) {
    vector<int> tSensorIDs;

    int len = 0;
    // GPU temperature sensors
    unsigned char *tSensors = nullptr;
    int ret = XNVCTRLQueryTargetBinaryData(*dw, NV_CTRL_TARGET_TYPE_GPU, i, 0,
                                           NV_CTRL_BINARY_DATA_THERMAL_SENSORS_USED_BY_GPU, &tSensors, &len);
    if (!ret || tSensors == nullptr) {
      LOG(severity_level::error) << "Failed to query number of temperature sensors for GPU: " << i;
      return uids;
    }
    tSensorIDs = nvProcessBinaryData(tSensors, len);

    for (const auto &tsi : tSensorIDs)
      uids.emplace_back(UID(Util::nvidia_label, tsi, string(Util::temp_sensor_label)));
  }

  return uids;
}

bool SensorController::nvidiaSupported() {
//  dw = DisplayWrapper(":0");
  if (!dw.open()) {
    LOG(severity_level::debug) << "X11 display cannot be opened";
    return false;
  }

  int eventBase, errorBase;
  auto ret = XNVCTRLQueryExtension(*dw, &eventBase, &errorBase);
  if (!ret) {
    LOG(severity_level::debug) << "NVIDIA fan control not supported";   // NV-CONTROL X does not exist!
    return false;
  } else if (errorBase)
    LOG(severity_level::warning) << "NV-CONTROL X return error base: " << errorBase;

  int major = 0, minor = 0;
  ret = XNVCTRLQueryVersion(*dw, &major, &minor);
  if (!ret) {
    LOG(severity_level::error) << "Failed to query NV-CONTROL X version";
    return false;
  } else if ((major < 1) || (major == 1 && minor < 9))
    LOG(severity_level::warning) << "NV-CONTROL X version is not officially supported (too old!)";

  LOG(severity_level::debug) << "NVIDIA fan control supported";
  return true;
}

void SensorController::enableNvidiaFanControlCoolbit() {
  // TODO: find and set Coolbits value without 'nvidia-xconfig' - for when not available (e.g. in snap confinement)
  string command("sudo nvidia-xconfig -t | grep Coolbits");
  redi::pstream ips(command);
  string l;
  std::getline(ips, l);
  int iv = (!l.empty()) ? Util::getLastNum(l) : 0;  // initial value
  int cv(iv);   // current

  const int nBits = 5;
  const int fcBit = 2;    // 4
  int cbV[nBits] = {1, 2, 4, 8, 16};  // pow(2, bit)
  int cbS[nBits]{0};

  for (auto i = nBits - 1; i >= 0; --i)
    if ((cbS[i] = (cv / cbV[i]) >= 1))
      cv -= cbV[i];

  int nv = iv;
  if (cv > 0) {
    LOG(severity_level::error) << "Invalid coolbits value, fixing with fan control bit set";
    nv -= cv;
  }

  if (!cbS[fcBit])
    nv += cbV[fcBit];

  if (nv != iv) {
    command = string("sudo nvidia-xconfig --cool-bits=") + to_string(nv) + " > /dev/null";
    if (system(command.c_str()) != 0)
      LOG(severity_level::error) << "Failed to write coolbits value, nvidia fan test may fail!";
    LOG(severity_level::info) << "Reboot, or restart your display server to enable NVIDIA fan control";
  }
}
#endif //FANCON_NVIDIA_SUPPORT

SensorsWrapper::SensorsWrapper() {
  sensors_init(NULL);

  const sensors_chip_name *cn = nullptr;
  int ci = 0;
  while ((cn = sensors_get_detected_chips(NULL, &ci)) != NULL)
    chips.push_back(cn);
}