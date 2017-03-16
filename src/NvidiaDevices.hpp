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

  int readRPM() { return NV::rpm.read(hwID); };
  int readPWM() { return percentToPWM(NV::pwm_percent.read(hwID)); }
  void writePWM(int pwm) { NV::pwm_percent.write(hwID, pwmToPercent(pwm)); }
  void writeEnableMode(int mode) { NV::enable_mode.write(hwID, mode); }

private:
  static int pwmToPercent(int pwm);
  static int percentToPWM(int percent);
};

class SensorParentNV : public SensorParentInterface {
public:
  SensorParentNV(const UID &uid) : hwID(uid.hwID) {}

  int hwID;

  bool operator==(const UID &other) const { return hwID == other.hwID; }

  inline int read() { return NV::temp.read(hwID); };
};
}
#endif //FANCON_NVDEVICES_HPP
#endif //FANCON_NVIDIA_SUPPORT