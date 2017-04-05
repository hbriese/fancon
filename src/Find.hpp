#ifndef FANCON_FIND_HPP
#define FANCON_FIND_HPP

#include <memory>       // unique_ptr
#include <vector>
#include <sensors/sensors.h>
#include "Util.hpp"
#include "UID.hpp"
#include "Config.hpp"
#include "Fan.hpp"
#include "SensorInterface.hpp"
#include "NvidiaDevices.hpp"

using fancon::Util::lastNum;
using fancon::UID;
using fancon::FanInterface;
using fancon::Fan;
using fancon::FanNV;
using fancon::SensorInterface;
using fancon::Sensor;
using fancon::SensorNV;

namespace {
struct SensorsWrapper {
  SensorsWrapper();
  ~SensorsWrapper() { sensors_cleanup(); }
  vector<const sensors_chip_name *> chips;  // Do *not* free, use sensors_cleanup()
};
}

namespace fancon {
class Find {
public:
  static unique_ptr<FanInterface> getFan(const UID &uid, const Config &fanConf = Config(), bool dynamic = true);
  static vector<UID> getFanUIDs();

  static unique_ptr<SensorInterface> getSensor(const UID &uid);
  static vector<UID> getSensorUIDs();

private:
  static vector<UID> getHWMonUIDs(const char *devicePathSuffix);
};
}

#endif //FANCON_FIND_HPP