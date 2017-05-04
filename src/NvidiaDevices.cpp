#ifdef FANCON_NVIDIA_SUPPORT
#include "NvidiaDevices.hpp"

using namespace fancon;

FanNV::FanNV(const UID &fanUID, const fan::Config &conf, bool dynamic)
    : FanInterface(fanUID, conf, dynamic,
                   NV_CTRL_GPU_COOLER_MANUAL_CONTROL_FALSE,
                   NV_CTRL_GPU_COOLER_MANUAL_CONTROL_TRUE) {
  if (!points.empty())
    writeEnableMode(manual_enable_mode);
}

void FanNV::writePWM(const pwm_t &pwm) {
  // Attempt to recover control of the device if the write fails
  if (!NV::pwm_percent.write(hw_id, pwmToPercent(pwm)))
    FanInterface::recoverControl(string("NVIDIA fan ") + hw_id_str);
}

bool FanNV::writeEnableMode(const enable_mode_t &mode) {
  return NV::enable_mode.write(hw_id, mode);
}

percent_t FanNV::pwmToPercent(const pwm_t &pwm) {
  return static_cast<percent_t>((static_cast<double>(pwm) / pwm_max_abs) * 100);
}

pwm_t FanNV::percentToPWM(const percent_t &percent) {
  return static_cast<pwm_t>(static_cast<double>(percent / 100) * pwm_max_abs);
}

bool SensorNV::operator==(const UID &other) const {
  return hw_id == other.hw_id;
}

#endif // FANCON_NVIDIA_SUPPORT
