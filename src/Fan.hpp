#ifndef FANCTL_FAN_HPP
#define FANCTL_FAN_HPP

#include <algorithm>    // lower_bound
#include <array>
#include <chrono>       // system_clock::now, duration
#include <iterator>     // next, prev, distance
#include <thread>       // this_thread::sleep
#include <utility>      // pair, make_pair
#include <boost/filesystem.hpp>
#include "Util.hpp"
#include "UID.hpp"
#include "Config.hpp"
#include "TemperatureSensor.hpp"

namespace chrono = std::chrono;
namespace bfs = boost::filesystem;

using std::move;
using boost::filesystem::path;
using boost::filesystem::exists;
using fanctl::Util::read;
using fanctl::Util::write;
using fanctl::Util::log;
using fanctl::Util::coutThreadsafe;
using fanctl::UID;
using fanctl::Config;

namespace fanctl {
struct TestResult {
  TestResult() { can_test = false; }

  TestResult(int rpm_min, int rpm_max, int pwm_min, int pwm_max, int pwm_start, long stop_time, double slope)
      : rpm_min(rpm_min), rpm_max(rpm_max), pwm_min(pwm_min), pwm_max(pwm_max), pwm_start(pwm_start),
        stop_time(stop_time), slope(slope) {}
  int rpm_min,
      rpm_max,
      pwm_min,
      pwm_max,
      pwm_start;
  long stop_time;
  double slope;

  bool can_test = false;

  const bool testable() { return can_test; }

  const bool valid() {
    return (rpm_min > 0) && (rpm_min < rpm_max) && (pwm_min > 0) &&
        (pwm_min < pwm_max) && (pwm_start > 0) && (slope > 0);
  }
};

class Fan {
public:
  Fan(const UID &fan_uid, const Config &conf, bool dynamic = true);
  ~Fan();

  inline const int calcPWM(int rpm) { return (int) (((rpm - rpm_min) / slope) + pwm_min); }
  void update(int temp);
  int testPWM(int rpm);    // TODO: remove

  static TestResult test(const UID &fanUID, const bool profile);
  static void writeTestResult(const UID &fanUID, const TestResult &result);

  constexpr static const char *path_pf = "fan";

private:
  const string hwID;  // hwmon id
  vector<fanctl::Point> points;

  static const int manual_enable_mode = 1;
  int driver_enable_mode;
  bool dynamic;

  string pwm_p, rpm_p;
  string enable_pf;

  int rpm_min, rpm_max;
  int pwm_min, pwm_start;
  double slope;       // i.e. rpm-per-pwm     // TODO: replace with polynomial

  // values in ms
  // TODO: handle step up time and step down time
  long step_up_t, step_down_t,    // time taken before rpm is increased/decreased (set by user/driver)
      stop_t;
  static const int speed_change_t = 3;    // seconds to allow for rpm changes when altering the pwm
  static const int pwm_max_absolute = 255;

  static void sleep(int s);
};

struct FanPaths {
  FanPaths(const UID &fanUID);

  string pwm_pf, rpm_pf,
      enable_pf,
      pwm_min_pf, pwm_max_pf,
      rpm_min_pf, rpm_max_pf,
      pwm_start_pf, slope_pf,
      step_ut_pf, step_dt_pf, stop_t_pf;
};
}   // fanctl

#endif //FANCTL_FAN_HPP
