#ifndef fancon_SENSORCONTROLLER_HPP
#define fancon_SENSORCONTROLLER_HPP

#include <algorithm>    // search, find_if
#include <iostream>     // skipws, endl
#include <utility>      // move, pair
#include <numeric>      // iota
#include <memory>       // unique_ptr
#include <sstream>      // stringstream, ostream
#include <vector>
#include <sensors/sensors.h>
#include "Util.hpp"
#include "UID.hpp"
#include "Config.hpp"
#include "Fan.hpp"
#include "SensorParentInterface.hpp"

#ifdef FANCON_NVIDIA_SUPPORT
#include <X11/Xlib.h>   // Bool, Display
#include <NVCtrl/NVCtrlLib.h>
#include "NvidiaDevices.hpp"
using fancon::dw;   // DisplayWrapper
#endif //FANCON_NVIDIA_SUPPORT

using std::endl;
using std::search;
using std::string;
using std::skipws;
using std::find_if;
using std::unique_ptr;
using std::make_unique;
using std::move;
using std::istringstream;
using std::ostringstream;
using std::pair;
using std::vector;
using fancon::UID;
using fancon::Fan;
using fancon::SensorControllerConfig;
using fancon::FanConfig;
using fancon::SensorParentInterface;
using fancon::Util::getLastNum;
using fancon::Util::validIter;

namespace fancon {
struct SensorsWrapper;

class SensorController {
public:
  SensorController(uint nThreads = 0);

  fancon::SensorControllerConfig conf;

  vector<UID> getFans();
  vector<UID> getSensors();

  void writeConf(const string &path);
  vector<unique_ptr<SensorParentInterface>> readConf(const string &path);

private:
  bool nvidia_control = false;

  vector<UID> getUIDs(const char *devicePathPostfix);
  bool skipLine(const string &line);

#ifdef FANCON_NVIDIA_SUPPORT
  int getNVGPUs();
  vector<int> nvProcessBinaryData(const unsigned char *data, const int len);
  vector<UID> getFansNV();
  vector<UID> getSensorsNV();
  bool nvidiaSupported();
  static void enableNvidiaFanControlCoolbit();    // doesn't work when confined
#endif //FANCON_NVIDIA_SUPPORT
};

struct SensorsWrapper {
  SensorsWrapper();
  ~SensorsWrapper() { sensors_cleanup(); }
  vector<const sensors_chip_name *> chips;
};
}

#endif //fancon_SENSORCONTROLLER_HPP
