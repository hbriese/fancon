#ifndef FANCON_FANSYSFS_HPP
#define FANCON_FANSYSFS_HPP

#include "FanInterface.hpp"
#include "Util.hpp"

using control_flag_t = int;

namespace fc {
// https://www.kernel.org/doc/Documentation/hwmon/sysfs-interface
class FanSysfs : public FanInterface {
public:
  FanSysfs() = default;
  FanSysfs(string label_, const path &adapter_path_, int id_);
  ~FanSysfs() override;

  void test(shared_ptr<double> completed) override;
  bool enable_control() const override;
  bool disable_control() const override;

  void from(const fc_pb::Fan &f, const SensorMap &sensor_map) override;
  void to(fc_pb::Fan &f) const override;
  bool valid() const override;
  string_view uid() const override;

protected:
  path pwm_path, rpm_path, enable_path;
  control_flag_t manual_flag = 1, driver_flag = 2;

  bool set_pwm(const Pwm pwm) const override;
  Pwm get_pwm() const override;
  Rpm get_rpm() const override;

  virtual void test_driver_enable_flag();
  static path get_pwm_path(const path &adapter_path, int dev_id);
  static path get_rpm_path(const path &adapter_path, int dev_id);
  static path get_enable_path(const path &adapter_path, int dev_id);
  bool enable_path_exists() const;
  static bool is_faulty(const path &adapter_path, int dev_id);
};
} // namespace fc

#endif // FANCON_FANSYSFS_HPP
