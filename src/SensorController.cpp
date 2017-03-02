#include "SensorController.hpp"

using namespace fancon;

SensorsWrapper::SensorsWrapper() {
  sensors_init(NULL);

  const sensors_chip_name *cn = nullptr;
  int ci = 0;
  while ((cn = sensors_get_detected_chips(NULL, &ci)) != NULL)
    chips.push_back(cn);
}

SensorController::SensorController(uint nThreads)
    : nvidia_support(checkNvidiaSupport()) {
//  if (nvidia_support)
//    enableNvidiaFanControlCoolbit();

  if (nThreads)
    conf.threads = nThreads;
}

vector<UID> SensorController::getFansNV() {
  vector<UID> uids;
  if (!nvidia_support)
    return uids;

  // Number of fans
  int nFans = 0;
  int ret = XNVCTRLQueryTargetCount(*dw, NV_CTRL_TARGET_TYPE_COOLER, &nFans);
  if (!ret) {
    LOG(severity_level::error) << "Failed to query number of NVIDIA fans";
    return uids;
  } else if (nFans <= 0) {
    LOG(severity_level::debug) << "No NVIDIA fans detected";
    return uids;
  }

  // Number of GPUs
  int nGPUs = 0;
  ret = XNVCTRLQueryTargetCount(*dw, NV_CTRL_TARGET_TYPE_GPU, &nGPUs);
  if (!ret) {
    LOG(severity_level::error) << "Failed to query number of NVIDIA GPUs";
    return uids;
  }

  struct NVGPU {
    NVGPU(const int gpuID = 0) : gpu_id(gpuID) {}
    int gpu_id;
    char *product_name = nullptr;
    unsigned char *fans = nullptr;
  };

  // there must always be nFans >= nGPUs
  if (nFans < nGPUs)
    nGPUs = nFans;

  vector<NVGPU> gpus(static_cast<unsigned long>(nGPUs));
  std::iota(gpus.begin(), gpus.end(), 0);   // 0, 1, 2...

  for (int i = 0; i < nGPUs; ++i) {
    // GPU product name
    ret = XNVCTRLQueryTargetStringAttribute(*dw, NV_CTRL_TARGET_TYPE_GPU, i, 0,
                                            NV_CTRL_STRING_PRODUCT_NAME, &(gpus[i].product_name));
    if (!ret) {
      LOG(severity_level::error) << "Failed to query gpu: " << i << " product name";
      return uids;
    }

    if (nFans != nGPUs) { // Fans owned by what GPUs
      int len = 0;
      ret = XNVCTRLQueryTargetBinaryData(*dw, NV_CTRL_TARGET_TYPE_GPU, i, 0,
                                         NV_CTRL_BINARY_DATA_COOLERS_USED_BY_GPU, &(gpus[i].fans), &len);
      if (!ret) {
        LOG(severity_level::error) << "Failed to query number of fans for GPU: " << i;
        return uids;
      } else if (&(gpus[i].fans[0]) == nullptr)
        LOG(severity_level::warning) << "No fans found for GPU: " << i;
    }

    string productName(gpus[i].product_name);
    auto nameBegIt = find_if(productName.begin(), productName.end(), [](const char &c) { return std::isdigit(c); });
    if (nameBegIt == productName.end())
      nameBegIt = productName.begin();

    std::replace(nameBegIt, productName.end(), ' ', '_');
    uids.emplace_back(UID(Util::nvidia_label, i, string(nameBegIt, productName.end())));
  }

  return uids;
}

vector<UID> SensorController::getFansAll() {
  auto fans = getFans();
  auto nvFans = getFansNV();
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
      << "# <Fan UID>     <Temperature Sensor UID>    <[temperature (°C): speed (RPM); PWM (0-255)]>" << endl;
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

vector<unique_ptr<TempSensorParent>> SensorController::readConf(const string &path) {
  ifstream ifs(path);
  vector<unique_ptr<TempSensorParent>> tsParents;

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
                         [&tsUID](const unique_ptr<TempSensorParent> &pp) -> bool { return (*pp) == tsUID; });

    if (tspIt == tsParents.end()) {   // make new TSP if missing
      tsParents.emplace_back(make_unique<TempSensorParent>(tsUID));
      tspIt = tsParents.end() - 1;
    }

    if (fanUID.type == FAN_NVIDIA && nvidia_support) {
      if (nvidia_support) {
        auto fn = make_unique<FanNV>(fanUID, fanConf, conf.dynamic);
        if (!fn->points.empty())
          (*tspIt)->fansNVIDIA.emplace_back(move(fn));
        else
          fn.reset();
      } else
        LOG(severity_level::debug) << "NVIDIA fan exists in config, but nvidia control is not supported";
    } else if (fanUID.type == FAN) {
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

bool SensorController::checkNvidiaSupport() {
//  dw = DisplayWrapper(":0");
  if (!dw.open()) {
    LOG(severity_level::debug) << "X11 display cannot be opened";
    return false;
  }

  int eventBase, errorBase;
  auto ret = XNVCTRLQueryExtension(*dw, &eventBase, &errorBase);
  if (!ret) {
    LOG(severity_level::debug) << "NV-CONTROL X does not exist!";
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