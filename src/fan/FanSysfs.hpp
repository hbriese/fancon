#ifndef FANCON_FANSYSFS_HPP
#define FANCON_FANSYSFS_HPP

#include "FanInterface.hpp"
#include "util/Util.hpp"

using SysfsID = uint;
using control_flag_t = int;

namespace fc {
// https://www.kernel.org/doc/Documentation/hwmon/sysfs-interface
class FanSysfs : public FanInterface {
public:
  FanSysfs() = default;
  FanSysfs(string label_, const path &adapter_path_, SysfsID id_);
  ~FanSysfs() override;

  bool test(ObservableNumber<int> &status) override;
  bool enable_control() override;
  bool disable_control() override;
  Pwm get_pwm() const override;
  Rpm get_rpm() const override;
  bool valid() const override;
  string hw_id() const override;
  virtual DevType type() const override;

  void from(const fc_pb::Fan &f, const SensorMap &sensor_map) override;
  void to(fc_pb::Fan &f) const override;

protected:
  path pwm_path, rpm_path, enable_path;
  control_flag_t manual_flag = 1, driver_flag = 2;

  bool set_pwm(const Pwm pwm) override;
  virtual void test_driver_enable_flag();

  static path get_pwm_path(const path &adapter_path, SysfsID dev_id);
  static path get_rpm_path(const path &adapter_path, SysfsID dev_id);
  static path get_enable_path(const path &adapter_path, SysfsID dev_id);
  static optional<path> get_sensor_enable_path(const path &adapter_path,
                                               SysfsID dev_id);
  static bool is_faulty(const path &adapter_path, SysfsID dev_id);
};
} // namespace fc

#endif // FANCON_FANSYSFS_HPP
