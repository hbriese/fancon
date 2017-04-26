#include "Config.hpp"

using fancon::InputValue;
namespace controller = fancon::controller;
namespace fan = fancon::fan;

// TODO: review beg != end
InputValue::InputValue(string &input, string::iterator &&begin, std::function<bool(const char &ch)> predicate)
    : beg(begin), end(std::find_if_not(beg, input.end(), predicate)), found(beg != input.end() && beg != end) {}

InputValue::InputValue(string &input, const string &sep, std::function<bool(const char &ch)> predicate)
    : InputValue(input, afterSeperator(input.begin(), input.end(), sep), move(predicate)) {}

InputValue::InputValue(string &input, const char &sep, std::function<bool(const char &ch)> predicate)
    : InputValue(input, afterSeperator(input.begin(), input.end(), sep), move(predicate)) {}

string::iterator
InputValue::afterSeperator(const string::iterator &&beg, const string::iterator &&end, const char &sep) {
  auto ret = find(beg, end, sep);
  if (ret != end)
    ++ret;

  return ret;
}

string::iterator
InputValue::afterSeperator(const string::iterator &&beg, const string::iterator &&end, const string &sep) {
  auto ret = search(beg, end, sep.begin(), sep.end());
  if (ret != end)
    std::advance(ret, sep.size());

  return ret;
}

ostream &controller::operator<<(ostream &os, const controller::Config &c) {
  using namespace fancon::serialization_constants::controller_config;
  os << interval_prefix << c.update_interval.count() << ' '
     << threads_prefix << c.max_threads << ' '
     << dynamic_prefix << ((c.dynamic) ? "true" : "false");

  return os;
}

istream &controller::operator>>(istream &is, controller::Config &c) {
  string in;
  std::getline(is, in);

  using namespace fancon::serialization_constants::controller_config;
  InputValue dynamic(in, dynamic_prefix, ::isalpha);
  InputValue interval(in, interval_prefix, ::isdigit);
  InputValue threads(in, threads_prefix, ::isdigit);
  InputValue update(in, update_prefix_deprecated, ::isdigit);  /// <\deprecated Use interval

  // Fail if no values are found
  if (!dynamic.found && !interval.found && !threads.found && !update.found) {
    // Set invalid values - see valid()
    c.update_interval = seconds(0);
    c.max_threads = 0;
    return is;
  }

  if (dynamic.found) {
    // Convert string to bool
    string dynamicStr;
    dynamic.setIfValid(dynamicStr);
    c.dynamic = (dynamicStr != "false" || dynamicStr != "0");
  }

  if (interval.found || update.found) {
    // chrono::duration doesn't define operator>>
    decltype(c.update_interval.count()) update_interval{0};

    if (interval.found)
      interval.setIfValid(update_interval);
    else {
      update.setIfValid(update_interval);
      LOG(llvl::warning) << update_prefix_deprecated << " in " << Util::config_path
                         << " is deprecated, and WILL BE REMOVED. Use " << interval_prefix;
    }

    if (update_interval > 0)
      c.update_interval = seconds(update_interval);
  }

  if (threads.found)
    threads.setIfValid(c.max_threads);

  return is;
}

ostream &fan::operator<<(ostream &os, const fan::Point &p) {
  using namespace fancon::serialization_constants::point;
  os << p.temp
     << (p.validRPM() ? string() + rpm_separator + to_string(p.rpm) : "")
     << (p.validPWM() ? string() + pwm_separator + to_string(p.pwm) : "");

  return os;
}

istream &fan::operator>>(istream &is, fan::Point &p) {
  using namespace fancon::serialization_constants::point;
  string in;
  is >> std::skipws >> in;
  if (in.empty())
    return is;

  InputValue pwm(in, pwm_separator, ::isdigit);
  InputValue rpm(in, rpm_separator, ::isdigit);

  // Temp must be before (first of) PWM & RPM
  auto &&tempEnd = std::prev((rpm.found && rpm.beg < pwm.beg) ? rpm.beg : pwm.beg);
  InputValue temp(in, find_if(in.begin(), tempEnd, ::isdigit), ::isdigit);

  // Must contain temp, and either a rpm or pwm value
  if (!temp.found || (!rpm.found & !pwm.found)) {
    LOG(llvl::error) << "Invalid fan config: " << in;
    return is;
  }

  // Set values if they are found & valid
  if (temp.found) {
    temp.setIfValid(p.temp);
    if (temp.end != in.end() && std::tolower(*temp.end) == fahrenheit)
      p.temp = static_cast<temp_t>((p.temp - 32) / 1.8);
  }

  if (pwm.found)
    pwm.setIfValid(p.pwm);
  else if (rpm.found) {
    rpm.setIfValid(p.rpm);
    if (rpm.end != in.end())
      p.is_rpm_percent = (std::tolower(*rpm.end) == percent);
  }

  return is;
}

ostream &fan::operator<<(ostream &os, const fan::Config &c) {
  for (const auto &p : c.points)
    os << p << ' ';

  return os;
}

istream &fan::operator>>(istream &is, fan::Config &c) {
  while (!is.eof()) {   // TODO: !is.fail() ??
    Point p;
    is >> p;
    if (p.valid())
      c.points.emplace_back(move(p));
  }

  return is;
}