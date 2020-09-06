#include "FanDell.hpp"

using namespace fc;

FanDell::FanDell(string label_, const path &adapter_path_, uint id_)
    : FanSysfs(move(label_), adapter_path_, id_) {}

FanDell::~FanDell() {
  if (!ignore)
    FanDell::disable_control();
}

bool FanDell::enable_control() const {
  if (!SMM::smm_send(mode(true))) {
    LOG(llvl::error) << *this
                     << ": failed to enable fan control, please "
                        "re-test or manually configure driver_flag";
    return false;
  }

  return true;
}

bool fc::FanDell::disable_control() const {
  if (!SMM::smm_send(mode(false))) {
    LOG(llvl::error) << *this
                     << ": failed to disable fan control, please "
                        "re-test or manually configure driver_flag";
    return false;
  }

  return true;
}

bool fc::FanDell::valid() const {
  if (!fc::FanSysfs::valid())
    return false;

  const auto id = Util::postfix_num<SysfsID>(pwm_path.c_str());
  return id && SMM::found() && SMM::fan_status(*id - 1) != SMM::FAN_NOT_FOUND;
}

DevType fc::FanDell::type() const { return DevType::DELL; }

void fc::FanDell::to(fc_pb::Fan &f) const {
  fc::FanSysfs::to(f);
  f.set_type(type());
}

void fc::FanDell::test_driver_enable_flag() {
  if (driver_flag >= 1 && driver_flag <= 3)
    return;

  bool found = false;
  for (const auto m : {SMM::SMM_MANUAL_CONTROL_2, SMM::SMM_MANUAL_CONTROL_1,
                       SMM::SMM_MANUAL_CONTROL_3}) {
    if (!SMM::smm_send(m))
      continue;

    // Choose the PWM, min or max, furthest away from the current
    const Pwm cur = get_pwm();
    const Pwm target_pwm =
        ((cur - PWM_MIN) > (PWM_MAX - cur)) ? PWM_MIN : PWM_MAX;

    set_pwm(target_pwm);
    sleep_for(milliseconds(1000));
    if (get_pwm() == target_pwm) {
      driver_flag = m;
      found = true;
      break;
    }
  }

  if (!found) { // Default to method 2
    driver_flag = 2;
    LOG(llvl::warning)
        << "Unable to determine DELL fan control method, defaulting to 2";
  }
}

SMM::Cmd fc::FanDell::mode(bool enable) const {
  switch (driver_flag) {
  case 1:
    return (enable) ? SMM::SMM_MANUAL_CONTROL_1 : SMM::SMM_AUTO_CONTROL_1;
  case 3:
    return (enable) ? SMM::SMM_MANUAL_CONTROL_3 : SMM::SMM_AUTO_CONTROL_3;
  default:
    LOG(llvl::warning) << *this << ": invalid driver flag (" << driver_flag
                       << "), defaulting to 2";
    [[fallthrough]];
  case 2:
    return (enable) ? SMM::SMM_MANUAL_CONTROL_2 : SMM::SMM_AUTO_CONTROL_2;
  }
}