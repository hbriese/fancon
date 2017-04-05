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

Fan::Paths::Paths(const UID &uid, const string &hwID) {
  const string devID(to_string(fancon::FanInterfacePaths::getDeviceID(uid)));

  string pwm_pf = fancon::FanInterfacePaths::pwm_prefix + devID;
  (pwm = fancon::Util::getPath(pwm_pf, hwID, uid.type, true)).shrink_to_fit();
  (enable_pf = pwm_pf + "_enable").shrink_to_fit();

  string rpm_pf = fancon::FanInterfacePaths::rpm_prefix + devID + "_input";
  (rpm = fancon::Util::getPath(rpm_pf, hwID, uid.type, true)).shrink_to_fit();
}