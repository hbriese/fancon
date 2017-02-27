#ifndef FANCON_FANINTERFACE_HPP
#define FANCON_FANINTERFACE_HPP

#include <sstream>    // stringstream
#include <functional>   // function
#include "Util.hpp"
#include "UID.hpp"
#include "Config.hpp"

using std::stringstream;
using std::function;
using fancon::UID;
using fancon::FanConfig;
using fancon::Util::read;
using fancon::Util::write;
using fancon::Util::log;
using fancon::Util::getPath;
using fancon::Util::speed_change_t;
using fancon::Util::pwm_max_absolute;

namespace fancon {
struct FanTestResult {
  FanTestResult() { canTest = false; }
  FanTestResult(int rpm_min, int rpm_max, int pwm_min, int pwm_max, int pwm_start, long stop_time, double slope)
      : rpm_min(rpm_min), rpm_max(rpm_max), pwm_min(pwm_min), pwm_max(pwm_max), pwm_start(pwm_start),
        stop_time(stop_time), slope(slope) {}

  int rpm_min,
      rpm_max,
      pwm_min,
      pwm_max,
      pwm_start;
  long stop_time;
  double slope;

  bool canTest = true;

  bool testable() { return canTest; }

  bool valid() {
    return (rpm_min > 0) && (rpm_min < rpm_max) && (pwm_min > 0) && (pwm_max <= 255)
        && (pwm_min < pwm_max) && (pwm_start > 0) && (slope > 0);
  }
};

namespace tests {
FanTestResult runTests(function<int()> rPWM, function<void(int)> wPWM, function<int()> rRPM,
                       function<void(int)> wEnableMode, int manualEnableMode, int prevEnableMode);

int getMaxRPM(function<void(int)> wPWM, function<int()> rRPM);
int getMaxPWM(function<void(int)> wPWM, function<int()> rRPM, const int rpm_max);
long getStopTime(function<void(int)> wPWM, function<int()> rRPM);
int getPWMStart(function<int()> rPWM, function<void(int)> wPWM, function<int()> rRPM);
std::pair<int, int> getPWMRPMMin(function<void(int)> wPWM, function<int()> rRPM, int startPWM);
}

class FanInterface {
public:
  FanInterface(const UID &uid, const FanConfig &conf = FanConfig(), bool dynamic = true);

  bool tested = false;   // characteristic variables written
  int rpm_min, rpm_max,
      pwm_min, pwm_start;
  vector<fancon::Point> points;

  virtual int readRPM() { return 0; };
  virtual void writePWM(int pwm) { pwm = 0; };
  virtual void writeEnableMode(int mode) { mode = 0; }

  void verifyPoints(const UID &fanUID);

  inline int calcPWM(int rpm) { return (int) (((rpm - rpm_min) / slope) + pwm_min); }
  virtual void update(int temp);

  static void writeTestResult(const UID &uid, const FanTestResult &result, DeviceType devType);

protected:
  static const int manual_enable_mode = 1;
  const string hwID;

  long stop_t;
  double slope;   // i.e. rpm-per-pwm
  bool dynamic;
};

struct FanPaths {
  FanPaths(const UID &uid, DeviceType devType = FAN);

  DeviceType dev_type;

  string pwm_p, rpm_p, enable_pf;
  string pwm_min_pf, pwm_max_pf,
      rpm_min_pf, rpm_max_pf,
      pwm_start_pf, slope_pf,
      stop_t_pf;
  string hwmonID;

  bool tested() const;
};
}

#endif //FANCON_FANINTERFACE_HPP
