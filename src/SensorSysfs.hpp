#ifndef FANCON_SENSORSYSFS_HPP
#define FANCON_SENSORSYSFS_HPP

#include <algorithm>

#include "SensorInterface.hpp"

using control_flag_t = int;

namespace fc {
inline const Temp sysfs_temp_divisor = 1000;

// https://www.kernel.org/doc/Documentation/hwmon/sysfs-interface
class SensorSysfs : public SensorInterface {
public:
  SensorSysfs() = default;
  SensorSysfs(string label_, const string &device_path);

  optional<Temp> min_temp() const override;
  optional<Temp> max_temp() const override;
  bool valid() const override;
  string hw_id() const override;

  void from(const fc_pb::Sensor &s) override;
  void to(fc_pb::Sensor &s) const override;

private:
  path input_path, enable_path, fault_path, min_path, max_path, crit_path;

  Temp read() const override;

  bool enable() const;
  bool is_faulty();
  static path if_exists(const path &p);
};
} // namespace fc

#endif // FANCON_SENSORSYSFS_HPP
