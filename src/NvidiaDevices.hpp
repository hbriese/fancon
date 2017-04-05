#ifdef FANCON_NVIDIA_SUPPORT
#ifndef FANCON_NVDEVICES_HPP
#define FANCON_NVDEVICES_HPP

#include "FanInterface.hpp"
#include "SensorInterface.hpp"
#include "NvidiaUtil.hpp"

namespace fancon {
using percent_t = int;

class FanNV : public FanInterface {
public:
  FanNV(const UID &fanUID, const fan::Config &conf = fan::Config(), bool dynamic = true);
  ~FanNV() { writeEnableMode(driver_enable_mode); }

  rpm_t readRPM() { return NV::rpm.read(hw_id); };
  pwm_t readPWM() { return percentToPWM(NV::pwm_percent.read(hw_id)); }
  void writePWM(const pwm_t &pwm) {
    // Attempt to recover control of the device if the write fails
    if (!NV::pwm_percent.write<pwm_t>(hw_id, pwmToPercent(pwm)))
      FanInterface::recoverControl(string("NVIDIA fan ").append(hw_id_str));
  }
  bool writeEnableMode(const enable_mode_t &mode) { return NV::enable_mode.write<enable_mode_t>(hw_id, mode); }

private:
  static percent_t pwmToPercent(const pwm_t &pwm);
  static pwm_t percentToPWM(const percent_t &percent);
};

struct SensorNV : public SensorInterface {
  SensorNV(const UID &uid) : hw_id(uid.hw_id) {}

  int hw_id;

  bool operator==(const UID &other) const { return hw_id == other.hw_id; }

  temp_t read() { return NV::temp.read(hw_id); };
};
}

#endif //FANCON_NVDEVICES_HPP
#endif //FANCON_NVIDIA_SUPPORT