#ifndef FANCON_CONFIG_HPP
#define FANCON_CONFIG_HPP

#include <algorithm>    // find, search, remove_if
#include <cctype>       // isspace, isdigit
#include <exception>    // runtime_error
#include <iostream>     // skipws, cin
#include <string>       // to_string, stoi, stod
#include <sstream>      // ostream, istream
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
using fancon::Util::log;
using fancon::Util::isNum;
using fancon::Util::validIter;

namespace fancon {
class SensorControllerConfig {
public:
  SensorControllerConfig(bool dynamic = true, uint update_time_s = 2)
      : dynamic(dynamic), update_time_s(update_time_s) {}

  SensorControllerConfig(istream &is) { is >> *this; }

  bool dynamic;
  uint update_time_s;

  bool valid() { return update_time_s > 0; }

  SensorControllerConfig &operator=(const SensorControllerConfig &other) {  //= default;
    dynamic = other.dynamic;
    update_time_s = other.update_time_s;
    return *this;
  }

  friend ostream &operator<<(ostream &os, const SensorControllerConfig &c);
  friend istream &operator>>(istream &is, SensorControllerConfig &c);

private:
  const string dynamicBegSep = "dynamic=";
  const string updateBegSep = "update=";
};

ostream &operator<<(ostream &os, const fancon::SensorControllerConfig &c);
istream &operator>>(istream &is, fancon::SensorControllerConfig &c);

class Point {
public:
  Point(int temp = 0, int rpm = -1, int pwm = -1)
      : temp(temp), rpm(rpm), pwm(pwm) {}
  ~Point() {}

  Point &operator=(const Point &other);

  // TODO: add Fahrenheit support
  int temp,
      rpm,
      pwm;
  // TODO: use int val instead & a bool = true if pwm is found

  /* FORMAT:
   * [temp:RPM;PWM] [temp:RPM] [temp;PWM]
   * either ":RPM" or ";PWM" is required
   */

  inline const bool validPWM() const { return pwm >= 0; };
  inline const bool validRPM() const { return rpm >= 0; }
  const bool valid() const { return validRPM() || validPWM(); }

  friend ostream &operator<<(ostream &os, const Point &p);
  friend istream &operator>>(istream &is, Point &p);

private:
  const char tempBegSep = '[',
      rpmBegSep = ':',
      pwmBegSep = ';',
      endSep = ']';
};

ostream &operator<<(ostream &os, const fancon::Point &p);
istream &operator>>(istream &is, fancon::Point &p);

class Config {
public:
  Config(istream &is) { is >> *this; }

  ~Config() { points.clear(); }

  vector<Point> points;

  /* FORMAT:
   * ${Point} ${Point}...
   */

  const bool valid() const { return !points.empty(); }

  friend ostream &operator<<(ostream &os, const Config &c);
  friend istream &operator>>(istream &is, Config &c);
};

ostream &operator<<(ostream &os, const fancon::Config &c);
istream &operator>>(istream &is, fancon::Config &c);
}

#endif //FANCON_CONFIG_HPP