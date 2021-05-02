#include "FanSysfs.hpp"

fc::FanSysfs::FanSysfs(string label_, const path &adapter_path_, SysfsID id_)
    : FanInterface(move(label_)), pwm_path(get_pwm_path(adapter_path_, id_)),
      rpm_path(get_rpm_path(adapter_path_, id_)),
      enable_path(get_enable_path(adapter_path_, id_)) {
  if (is_faulty(adapter_path_, id_)) {
    LOG(llvl::warning) << *this << ": is faulty, ignoring";
    ignore = true;
    return;
  }

  // Enable the fan sensor
  const auto sensor_ep = get_sensor_enable_path(adapter_path_, id_);
  if (sensor_ep)
    Util::write(*sensor_ep, 1);
}

fc::FanSysfs::~FanSysfs() {
  if (enabled)
    fc::FanSysfs::disable_control();
}

bool fc::FanSysfs::enable_control() {
  const bool success =
      !(exists(enable_path)) || Util::write(enable_path, manual_flag);
  if (success)
    enabled = true;

  return success;
}

bool fc::FanSysfs::disable_control() {
  const bool success =
      !(exists(enable_path)) || Util::write(enable_path, driver_flag);
  if (success)
    enabled = false;

  return true;
}

bool fc::FanSysfs::test(ObservableNumber<int> &status) {
  test_driver_enable_flag();
  return fc::FanInterface::test(status);
}

void fc::FanSysfs::from(const fc_pb::Fan &f, const SensorMap &sensor_map) {
  fc::FanInterface::from(f, sensor_map);
  pwm_path = f.pwm_path();
  rpm_path = f.rpm_path();
  enable_path = f.enable_path();
  driver_flag = f.driver_flag();
}

void fc::FanSysfs::to(fc_pb::Fan &f) const {
  fc::FanInterface::to(f);
  f.set_type(type());
  f.set_pwm_path(pwm_path);
  f.set_rpm_path(rpm_path);
  f.set_enable_path(enable_path);
  f.set_driver_flag(driver_flag);
}

bool fc::FanSysfs::valid() const {
  const bool pe = fs::exists(pwm_path), re = fs::exists(rpm_path);
  if (pe && re)
    return true;

  LOG(llvl::warning) << *this << ": invalid, "
                     << Util::join({{!pe, "pwm_path"}, {!re, "rpm_path"}})
                     << " not configured or doesn't exist";
  return pe && re;
}

string fc::FanSysfs::hw_id() const { return pwm_path.c_str(); }

DevType fc::FanSysfs::type() const { return DevType::SYS; }

bool fc::FanSysfs::set_pwm(const Pwm pwm) {
  if (!Util::write(pwm_path, pwm) && !FanInterface::recover_control())
    return false;

  return FanInterface::set_pwm(pwm);
}

Pwm fc::FanSysfs::get_pwm() const {
  const auto pwm = Util::read<Pwm>(pwm_path);
  if (pwm) {
    return *pwm;
  } else {
    LOG(llvl::error) << *this << ": failed to get pwm";
    return 0;
  }
}

Rpm fc::FanSysfs::get_rpm() const {
  const auto rpm = Util::read<Rpm>(rpm_path);
  if (rpm) {
    return *rpm;
  } else {
    LOG(llvl::error) << *this << ": failed to get rpm";
    return 0;
  }
}

void fc::FanSysfs::test_driver_enable_flag() {
  if (exists(enable_path)) {
    // 0: no fan speed control (i.e. fan at full speed)
    // 1: manual fan speed control enabled
    // 2+: automatic fan speed control enabled
    const auto mode = Util::read<control_flag_t>(enable_path);
    if (mode && *mode != 0 && *mode != manual_flag)
      driver_flag = *mode;
  }
}

path fc::FanSysfs::get_pwm_path(const path &adapter_path, SysfsID dev_id) {
  return adapter_path / path(string("pwm") + to_string(dev_id));
}

path fc::FanSysfs::get_rpm_path(const path &adapter_path, SysfsID dev_id) {
  return adapter_path / path("fan" + to_string(dev_id) + "_input");
}

path fc::FanSysfs::get_enable_path(const path &adapter_path, SysfsID dev_id) {
  const path ep = get_pwm_path(adapter_path, dev_id).string() + "_enable";
  return (exists(ep)) ? ep : "";
}

optional<path> fc::FanSysfs::get_sensor_enable_path(const path &adapter_path,
                                                    SysfsID dev_id) {
  const path ep = adapter_path / path("fan" + to_string(dev_id) + "_enable");
  return (exists(ep)) ? optional(ep) : nullopt;
}

bool fc::FanSysfs::is_faulty(const path &adapter_path, SysfsID dev_id) {
  const path fault_p{adapter_path / path("fan" + to_string(dev_id) + "_fault")};
  return Util::read<int>(fault_p).value_or(0) > 0;
}
