#ifndef FANCON_CONFIG_HPP
#define FANCON_CONFIG_HPP

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
using std::stoi;
using std::ostream;
using std::istream;
using std::to_string;
using std::vector;
using std::chrono::seconds;
using std::chrono::milliseconds;
using fancon::Util::isNum;
using fancon::Util::equalTo;

namespace fancon {
using temp_t = int;

class InputValue {
public:
  InputValue(string &input, string::iterator &&begin, std::function<bool(const char &ch)> predicate);
  InputValue(string &input, const string &sep, std::function<bool(const char &ch)> predicate);
  InputValue(string &input, const char &sep, std::function<bool(const char &ch)> predicate);

  const string::iterator beg, end;
  const bool found;

  template<typename T>
  void setIfValid(T &value) {
    assert(found && "Input value must be found before trying to set");

    // TODO: C++17 - use from_chars
    std::istringstream iss(string(beg, end));
    T val;
    iss >> val;
    if (iss)
      value = val;
  }

private:
  string::iterator afterSeperator(const string::iterator &&beg, const string::iterator &&end, const char &sep);
  string::iterator afterSeperator(const string::iterator &&beg, const string::iterator &&end, const string &sep);
};

namespace controller {
class Config {
public:
  Config(bool dynamic = true, seconds updateInterval = seconds(2),
         uint maxThreads = std::thread::hardware_concurrency())
      : dynamic(dynamic), update_interval(updateInterval), max_threads(maxThreads) {}

  Config(istream &is)
      : Config() { is >> *this; }

  bool dynamic;
  seconds update_interval;
  uint max_threads;

  bool valid() { return update_interval.count() > 0 && max_threads > 0; }

  Config &operator=(const Config &other) {
    std::tie(dynamic, update_interval, max_threads) = std::tie(other.dynamic, other.update_interval, other.max_threads);
    return *this;
  }

  friend ostream &operator<<(ostream &os, const Config &c);
  friend istream &operator>>(istream &is, Config &c);
};

ostream &operator<<(ostream &os, const Config &c);
istream &operator>>(istream &is, Config &c);
}

namespace fan {
using pwm_t = int;
using rpm_t = int;

constexpr static const pwm_t pwm_min_abs = 0, pwm_max_abs = 255;
constexpr static const rpm_t rpm_min_abs = 0;

/// \note
/// Format: [tempF:RPM%;PWM] e.g. [temp:RPM] [temp;PWM] <br>
/// Required: ":RPM" or ";PWM" <br>
/// Optional: f (fahrenheit) and % (percent)
class Point {
public:
  Point(temp_t temp = 0, rpm_t rpm = (rpm_min_abs - 1), pwm_t pwm = (pwm_min_abs - 1), bool isRpmPercent = false)
      : temp(temp), rpm(rpm), pwm(pwm), is_rpm_percent(isRpmPercent) {}

  temp_t temp;
  rpm_t rpm;    // TODO: C++17: replace with std::variant (tagged union)
  pwm_t pwm;
  bool is_rpm_percent;

  bool validPWM() const { return pwm >= pwm_min_abs; }
  bool validRPM() const { return rpm >= rpm_min_abs; }
  bool valid() const { return validRPM() || validPWM(); }

  friend ostream &operator<<(ostream &os, const Point &p);
  friend istream &operator>>(istream &is, Point &p);
};

ostream &operator<<(ostream &os, const Point &p);
istream &operator>>(istream &is, Point &p);

/// \brief
/// List of points <br> I/O is sequential
class Config {
public:
  Config() {}
  Config(istream &is) { is >> *this; }

  vector<Point> points;

  bool valid() const { return !points.empty(); }

  friend ostream &operator<<(ostream &os, const Config &c);
  friend istream &operator>>(istream &is, Config &c);
};

ostream &operator<<(ostream &os, const Config &c);
istream &operator>>(istream &is, Config &c);
}

namespace serialization_constants {   // TODO: Review name
namespace controller_config {
const string
    dynamic_prefix = "dynamic=",
    interval_prefix = "interval=",
    threads_prefix = "threads=";
const string update_prefix_deprecated = "update=";  /// <\deprecated Use interval_prefix  // TODO: remove 08/17
}

namespace point {
constexpr const char
    rpm_separator = ':',
    pwm_separator = ';',
    fahrenheit = 'f',
    percent = '%';
}
}
}

#endif //FANCON_CONFIG_HPP