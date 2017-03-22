#ifndef fancon_CONTROLLER_HPP
#define fancon_CONTROLLER_HPP

#include <algorithm>    // search, find_if
#include <ios>          // skipws
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
#include "SensorInterface.hpp"

#ifdef FANCON_NVIDIA_SUPPORT
#include "NvidiaDevices.hpp"
using fancon::NV::dw;   // DisplayWrapper
#endif //FANCON_NVIDIA_SUPPORT

using std::search;
using std::skipws;
using std::find_if;
using std::istringstream;
using std::this_thread::sleep_for;
using std::chrono::seconds;
using std::chrono::milliseconds;
using fancon::UID;
using fancon::Fan;
using fancon::ControllerConfig;
using fancon::FanConfig;
using fancon::SensorInterface;
using fancon::Util::getLastNum;

namespace fancon {
struct SensorsWrapper;
struct Daemon;
struct MappedFan;

class Controller {
public:
  Controller(uint nThreads = 0);

  fancon::ControllerConfig conf;

  vector<UID> getFans();
  vector<UID> getSensors();

  static unique_ptr<SensorInterface> getSensor(const UID &uid);
  static unique_ptr<FanInterface> getFan(const UID &uid, const FanConfig &fanConf = FanConfig(), bool dynamic = true);

  void writeConf(const string &path);

  unique_ptr<Daemon> loadDaemon(const string &configPath, DaemonState &daemonState);

private:
  vector<UID> getUIDs(const char *devicePathPostfix);
  bool skipLine(const string &line);

#ifdef FANCON_NVIDIA_SUPPORT
  bool nvidia_control = false;
#endif //FANCON_NVIDIA_SUPPORT
};

struct SensorsWrapper {
  SensorsWrapper();
  ~SensorsWrapper() { sensors_cleanup(); }
  vector<const sensors_chip_name *> chips;  // do *not* use smart ptr, due to sensors_cleanup()
};

struct Daemon {
  Daemon(DaemonState &daemonState, const seconds updateTime)
      : state((daemonState = DaemonState::RUN)), update_time(updateTime) {}

  DaemonState &state;
  chrono::seconds update_time;

  vector<unique_ptr<SensorInterface>> sensors;
  vector<unique_ptr<MappedFan>> fans;

  void readSensors(vector<unique_ptr<SensorInterface>>::iterator &beg,
                   vector<unique_ptr<SensorInterface>>::iterator &end);
  void updateFans(vector<unique_ptr<MappedFan>>::iterator &beg,
                  vector<unique_ptr<MappedFan>>::iterator &end);
};

struct MappedFan {
  MappedFan(unique_ptr<FanInterface> fan, const int &temp) : fan(move(fan)), temp(temp) {}

  unique_ptr<FanInterface> fan;
  const int &temp;
};
}

#endif //fancon_CONTROLLER_HPP
