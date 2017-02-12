#ifndef FANCOND_FAN_HPP
#define FANCOND_FAN_HPP

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

using boost::filesystem::path;
using boost::filesystem::exists;
using fancon::Util::read;
using fancon::Util::write;
using fancon::Util::log;
using fancon::Util::coutThreadsafe;
using fancon::UID;
using fancon::Config;

namespace fancon {
struct TestResult {
  TestResult() { failure = true; }

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

  bool failure = false;

  const bool fail() { return failure; }

  const bool valid() {
    return (rpm_min > 0) && (rpm_min < rpm_max) && (pwm_min > 0) &&
        (pwm_min < pwm_max) && (pwm_start > 0) && (slope > 0);
  }
};

class Fan {
public:
  Fan(const UID &fan_uid, const Config &conf, bool dynamic = true);
  ~Fan();

  Fan &operator=(const Fan &other) = default;
  /*{
      if (this != &other) {
          points = other.points;
          pwm_p = other.pwm_p;
          rpm_p = other.rpm_p;
          rpm_min = other.rpm_min;
          rpm_max = other.rpm_max;
          pwm_start = other.pwm_start;
          slope = other.slope;
          step_up_t = other.step_up_t;
          step_down_t = other.step_down_t;
          stop_t = other.stop_t;
      }

      return *this;
  } */

  inline const int calcPWM(int rpm) { return (int) (((rpm - rpm_min) / slope) + pwm_min); }     // + pwm_min
  void update(int temp);
  int testPWM(int rpm);    // TODO: remove

  // TODO: return test results struct, so averages can be taken
  static TestResult test(const UID &fanUID, const bool profile);
  static void writeTestResult(const UID &fanUID, const TestResult &result);

  constexpr static const char *path_pf = "fan";

private:
  const string hwID;  // hwmon id
  vector<fancon::Point> points;

  static const int manual_enable_mode = 1;
  int driver_enable_mode;
  bool dynamic;

  string pwm_p, rpm_p;
  string enable_pf;
//    string pwm_pf,   // rw
//        rpm_pf;  // ro

//    int pwm_min, pwm_max;   // TODO: don't store, just use to check config at init
  int rpm_min, rpm_max;
  int pwm_min, pwm_start;
  double slope;       // i.e. rpm-per-pwm     // TODO: replace with polynomial
  //    int rpm_floor;      // lowest fan pwm before rpm = 0

  // values in ms
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
}   // fancon

#endif //FANCOND_FAN_HPP
