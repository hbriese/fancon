#ifndef FANCON_FIND_HPP
#define FANCON_FIND_HPP

#include "Config.hpp"
#include "Fan.hpp"
#include "NvidiaDevices.hpp"
#include "SensorInterface.hpp"
#include "UID.hpp"
#include "Util.hpp"
#include <memory> // unique_ptr
#include <sensors/sensors.h>
#include <vector>

#ifdef FANCON_NVIDIA_SUPPORT
using fancon::FanNV;
using fancon::SensorNV;
#endif

using fancon::Util::lastNum;
using fancon::UID;
using fancon::FanInterface;
using fancon::Fan;
using fancon::SensorInterface;
using fancon::Sensor;

namespace {
/// \brief Wrapper for safely handling libsensor chips
struct SensorsWrapper {
  SensorsWrapper();
  ~SensorsWrapper() { sensors_cleanup(); }
  vector<const sensors_chip_name *> chips; 
};
}

namespace fancon {
class Devices {
public:
  static unique_ptr<FanInterface>
  getFan(const UID &uid, const Config &fanConf = Config(), bool dynamic = true);
  static vector<UID> getFanUIDs();

  static unique_ptr<SensorInterface> getSensor(const UID &uid);
  static vector<UID> getSensorUIDs();

private:
  static vector<UID> getHWMonUIDs(const char *devicePathSuffix);
};
}

#endif // FANCON_FIND_HPP
