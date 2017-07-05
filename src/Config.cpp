#include "Config.hpp"

using fancon::InputValue;
namespace controller = fancon::controller;
namespace fan = fancon::fan;
namespace scc = fancon::serialization_constants::controller_config;
namespace scp = fancon::serialization_constants::point;

InputValue::InputValue(string &input, string::iterator &&begin,
                       std::function<bool(const char &)> &&predicate)
    : beg(begin), end(std::find_if_not(beg, input.end(), predicate)),
      found(beg != input.end() && beg != end) {}

InputValue::InputValue(string &input, const string &sep,
                       std::function<bool(const char &ch)> predicate)
    : InputValue(input, afterSeperator(input.begin(), input.end(), sep),
                 move(predicate)) {}

InputValue::InputValue(string &input, const char &sep,
                       std::function<bool(const char &ch)> predicate)
    : InputValue(input, afterSeperator(input.begin(), input.end(), sep),
                 move(predicate)) {}

string::iterator InputValue::afterSeperator(const string::iterator &&beg,
                                            const string::iterator &&end,
                                            const char &sep) {
  auto ret = find(beg, end, sep);
  if (ret != end)
    ++ret;

  return ret;
}

string::iterator InputValue::afterSeperator(const string::iterator &&beg,
                                            const string::iterator &&end,
                                            const string &sep) {
  auto ret = search(beg, end, sep.begin(), sep.end());
  if (ret != end)
    std::advance(ret, sep.size());

  return ret;
}

ostream &controller::operator<<(ostream &os, const controller::Config &c) {
  os << scc::profile_prefix << c.profile << ' '
     << scc::interval_prefix
     // Ouput update_interval in seconds (ms / 10^3)
     << static_cast<float>(c.update_interval.count()) / 1000 << ' '
     << scc::threads_prefix << c.max_threads << ' '
     << scc::dynamic_prefix << ((c.dynamic) ? "true" : "false");

  return os;
}

istream &controller::operator>>(istream &is, controller::Config &c) {
  string in;
  std::getline(is, in);

  InputValue profile(in, scc::profile_prefix, ::isalpha);
  InputValue dynamic(in, scc::dynamic_prefix, ::isalpha);
  InputValue interval(in, scc::interval_prefix, ::isdigit);
  InputValue threads(in, scc::threads_prefix, ::isdigit);
  InputValue update(in, scc::update_prefix_deprecated, ::isdigit);

  // Fail if no values are found
  if (!profile.found && !dynamic.found && !interval.found &&
      !threads.found && !update.found) {
    // Set invalid values - see valid()
    c.update_interval = seconds(0);
    c.max_threads = 0;
    return is;
  }

  if (profile.found)
    profile.setIfValid(c.profile);

  if (dynamic.found) {
    // Convert string to bool
    string dynamicStr;
    dynamic.setIfValid(dynamicStr);
    c.dynamic = (dynamicStr != "false" || dynamicStr != "0");
  }

  if (interval.found || update.found) {
    // Convert floating point seconds to milliseconds
    float uInterval{0};

    if (interval.found)
      interval.setIfValid(uInterval);
    else {
      update.setIfValid(uInterval);
      LOG(llvl::warning)
        << scc::update_prefix_deprecated << " is deprecated; use "
        << scc::interval_prefix;
    }

    if (uInterval > 0)
      c.update_interval = milliseconds(
          static_cast<decltype(c.update_interval.count())>(uInterval * 1000));
  }

  if (threads.found)
    threads.setIfValid(c.max_threads);

  return is;
}

ostream &fan::operator<<(ostream &os, const fan::Point &p) {
  os << p.temp << ((p.validRPM()) ? scp::rpm_separator + to_string(p.rpm) : "")
     << ((p.validPWM()) ? scp::pwm_separator + to_string(p.pwm) : "");

  return os;
}

istream &fan::operator>>(istream &is, fan::Point &p) {
  string in;
  is >> std::skipws >> in;
  if (in.empty())
    return is;

  InputValue pwm(in, scp::pwm_separator, ::isdigit);
  InputValue rpm(in, scp::rpm_separator, ::isdigit);

  // Temp must be before (first of) PWM & RPM
  auto &&tempEnd = prev((rpm.found && rpm.beg < pwm.beg) ? rpm.beg : pwm.beg);
  InputValue temp(in, find_if(in.begin(), tempEnd, ::isdigit), ::isdigit);

  // Must contain temp, and either a rpm or pwm value
  if (!temp.found || (!rpm.found & !pwm.found)) {
    LOG(llvl::error) << "Invalid fan config: " << in;
    return is;
  }

  // Set values if they are found & valid
  if (temp.found) {
    temp.setIfValid(p.temp);

    // Support fahrenheit by converting to celsius
    if (temp.end != in.end() && std::tolower(*temp.end) == scp::fahrenheit)
      p.temp = static_cast<temp_t>((p.temp - 32) / 1.8);
  }

  if (pwm.found)
    pwm.setIfValid(p.pwm);
  else if (rpm.found) {
    rpm.setIfValid(p.rpm);
    if (rpm.end != in.end())
      p.is_rpm_percent = (std::tolower(*rpm.end) == scp::percent);
  }

  return is;
}

ostream &fan::operator<<(ostream &os, const fan::Config &c) {
  for (const auto &p : c.points)
    os << p << ' ';

  return os;
}

istream &fan::operator>>(istream &is, fan::Config &c) {
  while (!is.eof()) { // TODO !is.fail() ??
    Point p{};
    is >> p;
    if (p.valid())
      c.points.emplace_back(move(p));
  }

  return is;
}
