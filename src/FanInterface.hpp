#ifndef FANCON_FANINTERFACE_HPP
#define FANCON_FANINTERFACE_HPP

#include "Config.hpp"
#include "UID.hpp"
#include "Util.hpp"
#include <chrono>
#include <sstream> // stringstream
#include <thread>
#include <tuple>
#include <type_traits> // is_same

using std::this_thread::sleep_for;
using std::stringstream;
using fancon::UID;
using fancon::Util::read;
using fancon::Util::write;
using fancon::Util::getPath;

using namespace fancon::fan;

namespace chrono = std::chrono;

namespace fancon {
struct FanCharacteristics;

using TestResult = std::pair<const FanCharacteristics, bool>;
using enable_mode_t = int;
using slope_t = double;

enum class FanState { unknown, stopped, full_speed };

class FanInterface {
public:
  FanInterface(const UID &uid, const fan::Config &conf, bool dynamic,
               enable_mode_t driverEnableMode,
               enable_mode_t manualEnableMode = 1);
  virtual ~FanInterface() = default;

  vector<fan::Point> points;
  decltype(points)::iterator
  prev_it;

  pwm_t pwm_start;
  milliseconds wait_time;

  const enable_mode_t manual_enable_mode;
  enable_mode_t driver_enable_mode;

  // TODO Functions don't need to be public
  virtual rpm_t readRPM() = 0;
  virtual rpm_t readPWM() = 0;
  virtual void writePWM(const pwm_t &pwm) = 0;
  virtual bool writeEnableMode(const enable_mode_t &mode) = 0;
  bool recoverControl(const string &deviceLabel);

  void update(const temp_t temp);

  TestResult test();
  static void writeTestResult(const UID &uid, const FanCharacteristics &result,
                              DeviceType devType);

protected:
  const int hw_id;
  const string hw_id_str;

  const bool dynamic;

  pwm_t calcPWM(const rpm_t &rpmMin, const pwm_t &pwmMin,
                const slope_t &slope, const rpm_t &rpm);
  void validatePoints(const UID &fanUID, const bool &tested,
                      const FanCharacteristics &c);

  pwm_t testPWM(const FanCharacteristics &c, const rpm_t &rpm); // TODO remove

  rpm_t getMaxRPM(FanState &state, const milliseconds &waitTime);
  milliseconds getMaxSpeedChangeTime(FanState &state, const rpm_t &rpmMax);
  pwm_t getMaxPWM(FanState &state, const milliseconds &waitTime,
                  const rpm_t &rpmMax);
  pwm_t getPWMStart(FanState &state, const milliseconds &waitTime);
  tuple<pwm_t, rpm_t> getMinAttributes(FanState &state,
                                       const milliseconds &waitTime,
                                       const pwm_t &startPWM);
};

struct FanCharacteristicPaths {
  explicit FanCharacteristicPaths(const UID &uid);

  DeviceType type;

  constexpr static const char *pwm_prefix = "pwm", *rpm_prefix = "fan";
  string pwm_min_pf, pwm_max_pf, pwm_start_pf,
      rpm_min_pf, rpm_max_pf, slope_pf, wait_time_pf;
  const string hw_id;

  bool tested() const;
  static int getDeviceID(const UID &uid) { return Util::lastNum(uid.dev_name); }
};

struct FanCharacteristics {
  FanCharacteristics() = default;
  FanCharacteristics(rpm_t rpmMin, rpm_t rpmMax, pwm_t pwmStart, pwm_t pwmMin,
                     pwm_t pwmMax, slope_t slope, milliseconds waitTime)
      : rpm_min(rpmMin), rpm_max(rpmMax), pwm_start(pwmStart),
        pwm_min(pwmMin), pwm_max(pwmMax), slope(slope), wait_time(waitTime) {}

  rpm_t rpm_min, rpm_max;
  pwm_t pwm_start, pwm_min{fancon::fan::pwm_min_abs}, pwm_max;
  slope_t slope;
  milliseconds wait_time;

  bool valid() const {
    return (rpm_min > 0) && (rpm_min < rpm_max) && (pwm_min > 0) &&
        (pwm_max <= 255) && (pwm_min < pwm_max) && (pwm_start > 0) &&
        (slope > 0);
  }
};
}

#endif // FANCON_FANINTERFACE_HPP
