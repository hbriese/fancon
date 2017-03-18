#ifndef fancon_CONFIG_HPP
#define fancon_CONFIG_HPP

#include <algorithm>    // find, search, remove_if
#include <cctype>       // isspace, isdigit
#include <exception>    // runtime_error
#include <iostream>     // skipws, cin
#include <string>       // to_string, stoi, stod
#include <sstream>      // ostream, istream
#include <thread>
#include <vector>
#include "Util.hpp"

using std::find;
using std::string;
using std::search;
using std::skipws;
using std::stoi;
using std::ostream;
using std::istream;
using std::to_string;
using std::vector;
using fancon::Util::coutThreadsafe;
using fancon::Util::isNum;
using fancon::Util::validIters;

namespace fancon {
class SensorControllerConfig {
public:
  SensorControllerConfig() {}
  SensorControllerConfig(istream &is) { is >> *this; }

  bool dynamic = true;
  uint update_time_s = 2;
  uint threads = 1; // default to single-threaded

  bool valid() { return update_time_s > 0; }

  SensorControllerConfig &operator=(const SensorControllerConfig &other) {
    dynamic = other.dynamic;
    update_time_s = other.update_time_s;
    threads = other.threads;
    return *this;
  }

  friend ostream &operator<<(ostream &os, const SensorControllerConfig &c);
  friend istream &operator>>(istream &is, SensorControllerConfig &c);

private:
  const string dynamic_prefix = "dynamic=";
  const string update_prefix = "update=";
  const string threads_prefix = "threads=";
};

ostream &operator<<(ostream &os, const fancon::SensorControllerConfig &c);
istream &operator>>(istream &is, fancon::SensorControllerConfig &c);

class Point {
public:
  Point(int temp = 0, int rpm = -1, int pwm = -1)
      : temp(temp), rpm(rpm), pwm(pwm) {}

  Point &operator=(const Point &other);

  int temp,
      rpm,
      pwm;

  /* FORMAT:
   * [temp:RPM;PWM] [temp:RPM] [temp;PWM]
   * either ":RPM" or ";PWM" is required
   */

  bool validPWM() const { return pwm >= 0; }
  bool validRPM() const { return rpm >= 0; }
  bool valid() const { return validRPM() || validPWM(); }

  friend ostream &operator<<(ostream &os, const Point &p);
  friend istream &operator>>(istream &is, Point &p);

private:
  const char temp_bsep = '[',
      rpm_bsep = ':',
      pwm_bsep = ';',
      esep = ']',
      fahrenheit = 'f';
};

ostream &operator<<(ostream &os, const fancon::Point &p);
istream &operator>>(istream &is, fancon::Point &p);

class FanConfig {
public:
  FanConfig() {}
  FanConfig(istream &is) { is >> *this; }

  vector<Point> points;

  /* FORMAT:
   * ${Point} ${Point}...
   */

  bool valid() const { return !points.empty(); }

  friend ostream &operator<<(ostream &os, const FanConfig &c);
  friend istream &operator>>(istream &is, FanConfig &c);
};

ostream &operator<<(ostream &os, const fancon::FanConfig &c);
istream &operator>>(istream &is, fancon::FanConfig &c);
}

#endif //fancon_CONFIG_HPP