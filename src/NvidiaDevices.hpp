#ifdef FANCON_NVIDIA_SUPPORT
#ifndef FANCON_NVDEVICES_HPP
#define FANCON_NVDEVICES_HPP

#include "FanInterface.hpp"
#include "SensorParentInterface.hpp"
#include "NvidiaUtil.hpp"

namespace fancon {
class FanNV : public FanInterface {
public:
  FanNV(const UID &fanUID, const FanConfig &conf = FanConfig(), bool dynamic = true);
  ~FanNV() { writeEnableMode(driver_enable_mode); }

  int readRPM() { return NV::rpm.read(hw_id); };
  int readPWM() { return percentToPWM(NV::pwm_percent.read(hw_id)); }
  void writePWM(int pwm) { NV::pwm_percent.write(hw_id, pwmToPercent(pwm)); }
  void writeEnableMode(int mode) { NV::enable_mode.write(hw_id, mode); }

private:
  static int pwmToPercent(int pwm);
  static int percentToPWM(int percent);
};

class SensorParentNV : public SensorParentInterface {
public:
  SensorParentNV(const UID &uid) : hw_id(uid.hw_id) {}

  int hw_id;

  bool operator==(const UID &other) const { return hw_id == other.hw_id; }

  int read() { return NV::temp.read(hw_id); };
};
}
#endif //FANCON_NVDEVICES_HPP
#endif //FANCON_NVIDIA_SUPPORT