#include "Fan.hpp"

using namespace fancon;

Fan::Fan(const UID &fanUID, const fan::Config &conf, bool dynamic)
    : FanInterface(fanUID, conf, dynamic, 2), p(fanUID, hw_id_str) {
  // Correct assumed driver enable mode if it is not default
  enable_mode_t dem;
  if ((dem = readEnableMode()) > driver_enable_mode)
    driver_enable_mode = dem;

  if (!points.empty())
    writeEnableMode(manual_enable_mode);
}

bool Fan::writePWM(const pwm_t &pwm) {
  // Attempt to recover control of the device if the write fails
  if (!write(p.pwm, pwm))
    return FanInterface::recoverControl(p.pwm);

  return true;
}

bool Fan::writeEnableMode(const enable_mode_t &mode) {
  return write(p.enable_pf, hw_id_str, mode, DeviceType::fan, true);
}

enable_mode_t Fan::readEnableMode() {
  return read<enable_mode_t>(p.enable_pf, hw_id_str, DeviceType::fan, true);
}

Fan::Paths::Paths(const UID &uid, const string &hwID) {
  const string devID(to_string(FanCharacteristicPaths::getDeviceID(uid)));

  string pwm_pf = FanCharacteristicPaths::pwm_prefix + devID;
  (pwm = Util::getPath(pwm_pf, hwID, uid.type, true)).shrink_to_fit();
  (enable_pf = pwm_pf + "_enable").shrink_to_fit();

  string rpm_pf = FanCharacteristicPaths::rpm_prefix + devID + "_input";
  (rpm = Util::getPath(rpm_pf, hwID, uid.type, true)).shrink_to_fit();
}
