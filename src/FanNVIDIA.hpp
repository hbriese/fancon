#ifndef FANCON_FANNVIDIA_HPP
#define FANCON_FANNVIDIA_HPP

#include <pstreams/pstream.h>
#include "FanInterface.hpp"

namespace fancon {
class FanNVIDIA : public FanInterface {
public:
  FanNVIDIA(const UID &fanUID, const FanConfig &conf = FanConfig(), bool dynamic = true);
  ~FanNVIDIA() { writeEnableMode(0); }

  using FanInterface::update;

  static FanTestResult test(const UID &fanUID);

private:
  static int readNvVar(const string &hwID, const char *var, bool targetGPU = false);
  template<typename T>
  static void writeNvVar(const string &hwID, const char *var, T &val, bool targetGPU = false) {
    stringstream c;
    c << "nvidia-settings -a \"[" << ((targetGPU) ? "gpu:" : "fan:") << hwID << "]/" << var << '=' << val
      << "\" > /dev/null";
    system(c.str().c_str());
  }

  static int pwmToPercent(int pwm) { return static_cast<int>((static_cast<double>(pwm) / pwm_max_absolute) * 100); }
  static int percentToPWM(int percent) {
    return static_cast<int>(static_cast<double>(percent / 100) * pwm_max_absolute);
  }

  static int readRPM(const string &hwID) { return readNvVar(hwID, "GPUCurrentFanSpeedRPM"); }
  int readRPM() { return readRPM(hwID); };

  static int readPWMPercent(const string &hwID) { return readNvVar(hwID, "GPUCurrentFanSpeed"); }
  static void writePWMPercent(const string &hwID, int pwmPercent) { writeNvVar(hwID, "GPUTargetFanSpeed", pwmPercent); }

  static void writePWM(const string &hwID, int pwm) { writePWMPercent(hwID, percentToPWM(pwm)); }
  void writePWM(int pwm) { writePWM(hwID, pwm); }

  static int readEnableMode(const string &hwID) { return readNvVar(hwID, "GPUFanControlState", true); }
  static void writeEnableMode(const string &hwID, int mode) { writeNvVar(hwID, "GPUFanControlState", mode, true); }
  void writeEnableMode(int mode) { writeEnableMode(hwID, mode); }
};
}
#endif //FANCON_FANNVIDIA_HPP
