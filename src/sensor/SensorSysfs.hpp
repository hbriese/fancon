#ifndef FANCON_SENSORSYSFS_HPP
#define FANCON_SENSORSYSFS_HPP

#include <algorithm>

#include "Sensor.hpp"

using fc::Util::real_path;

using control_flag_t = int;

namespace fc {
static const Temp SYSFS_TEMP_DIVISOR = 1000;

// https://www.kernel.org/doc/Documentation/hwmon/sysfs-interface
class SensorSysfs : public Sensor {
public:
  SensorSysfs() = default;
  SensorSysfs(string label_, const string &dev_path);

  optional<Temp> min_temp() const override;
  optional<Temp> max_temp() const override;
  bool valid() const override;
  string hw_id() const override;

  void from(const fc_pb::Sensor &s) override;
  void to(fc_pb::Sensor &s) const override;

private:
  optional<path> input_path, enable_path, fault_path, min_path, max_path, crit_path;

  optional<Temp> read() const override;

  bool enable() const;
  bool is_faulty() const;
};
} // namespace fc

#endif // FANCON_SENSORSYSFS_HPP
