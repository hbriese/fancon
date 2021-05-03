#ifndef FANCON_DEVICES_HPP
#define FANCON_DEVICES_HPP

#include <sensors/sensors.h>

#include "dell/FanDell.hpp"
#include "fan/Fan.hpp"
#include "fan/FanSysfs.hpp"
#include "nvidia/NvidiaDevices.hpp"
#include "sensor/Sensor.hpp"
#include "sensor/SensorSysfs.hpp"
#include "util/Util.hpp"
#include "proto/DevicesSpec.pb.h"

using std::find_if;
using std::make_move_iterator;
using std::set;

namespace fc {
class SensorChips {
public:
  SensorChips();
  ~SensorChips();

  void enumerate(FanMap &fans, SensorMap &sensors);

private:
  vector<const sensors_chip_name *> chips;
};

class Devices {
public:
  Devices(const fc_pb::Devices &d);
  explicit Devices(bool enumerate = false);

  FanMap fans;
  SensorMap sensors;

  void from(const fc_pb::Devices &d);
  void to(fc_pb::Devices &d) const;
};

bool operator==(const fc_pb::Fan &l, const fc_pb::Fan &r);
bool operator==(const fc_pb::Sensor &l, const fc_pb::Sensor &r);
} // namespace fc

#endif // FANCON_DEVICES_HPP
