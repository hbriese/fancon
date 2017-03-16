#ifdef FANCON_NVIDIA_SUPPORT
#include "NvidiaDevices.hpp"

using namespace fancon;

FanNV::FanNV(const UID &fanUID, const FanConfig &conf, bool dynamic)
    : FanInterface(fanUID, conf, dynamic, 0) {
  if (!points.empty()) {
    // set manual mode
    writeEnableMode(manual_enable_mode);
    if (readRPM() <= 0)  // start fan if stopped
      writePWM(pwm_start);
  }
}

int FanNV::pwmToPercent(int pwm) { return static_cast<int>((static_cast<double>(pwm) / pwm_max_absolute) * 100); }

int FanNV::percentToPWM(int percent) { return static_cast<int>(static_cast<double>(percent / 100) * pwm_max_absolute); }
#endif //FANCON_NVIDIA_SUPPORT