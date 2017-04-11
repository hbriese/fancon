#include "Config.hpp"

using fancon::InputValue;
namespace controller = fancon::controller;
namespace fan = fancon::fan;

InputValue::InputValue(string &input, const string &sep, std::function<bool(const char &ch)> predicate) {
  beg = search(input.begin(), input.end(), sep.begin(), sep.end());
  if (beg != input.end())
    beg = next(beg, sep.size());

  finishConstruction(input, move(predicate));
}

InputValue::InputValue(string &input, const char &sep, std::function<bool(const char &ch)> predicate) {
  beg = find(input.begin(), input.end(), sep);
  if (beg != input.end())
    ++beg;

  finishConstruction(input, move(predicate));
}

void InputValue::finishConstruction(string &input, std::function<bool(const char &ch)> predicate) {
  end = find_if(beg, input.end(), predicate);
  found = (beg != input.end() && beg != end);
}

ostream &controller::operator<<(ostream &os, const controller::Config &c) {
  using namespace fancon::serialization_constants::controller_config;
  os << update_prefix << c.update_interval.count() << ' ' << threads_prefix << c.max_threads
     << ' ' << dynamic_prefix << ((c.dynamic) ? "true" : "false");
  return os;
}

istream &controller::operator>>(istream &is, controller::Config &c) {
  // Read whole line from istream
  std::istreambuf_iterator<char> eos;
  string in(std::istreambuf_iterator<char>(is), eos);

  using namespace fancon::serialization_constants::controller_config;
  InputValue dynamicVal(in, dynamic_prefix, [](const char &ch) { return !std::isalpha(ch); });
  InputValue updateVal(in, update_prefix, [](const char &ch) { return !std::isdigit(ch); });
  InputValue threadsVal(in, threads_prefix, [](const char &ch) { return !std::isdigit(ch); });
  InputValue updateValDeprecated(in, update_prefix_deprecated, [](const char &ch) { return !std::isdigit(ch); });

  // Fail if no values are found
  if (!dynamicVal.found && !updateVal.found && !threadsVal.found) {
    // Set invalid values - see valid()
    c.update_interval = seconds(0);
    c.max_threads = 0;
    return is;
  }

  if (dynamicVal.found) {
    // Convert string to bool
    string dynamicStr;
    dynamicVal.setIfValid(dynamicStr);
    c.dynamic = (dynamicStr != "false" || dynamicStr != "0");
  }

  if (updateVal.found || updateValDeprecated.found) {
    // chrono::duration doesn't define operator>>
    decltype(c.update_interval.count()) update_interval{0};

    if (updateVal.found)
      updateVal.setIfValid(update_interval);
    else {
      updateValDeprecated.setIfValid(update_interval);
      LOG(llvl::warning) << update_prefix_deprecated << " in " << Util::config_path
                         << " is deprecated, and WILL BE REMOVED. Use " << update_prefix;
    }

    if (update_interval > 0)
      c.update_interval = seconds(update_interval);
  }

  if (threadsVal.found)
    threadsVal.setIfValid(c.max_threads);

  return is;
}

fan::Point &fan::Point::operator=(const fan::Point &other) {
  temp = other.temp;
  rpm = other.rpm;
  pwm = other.pwm;

  return *this;
}

ostream &fan::operator<<(ostream &os, const fan::Point &p) {
  using namespace fancon::serialization_constants::point;
  string rpmOut, pwmOut;
  if (p.validRPM())
    rpmOut += rpm_separator + to_string(p.rpm);
  if (p.validPWM())
    pwmOut += pwm_separator + to_string(p.pwm);

  os << temp_separator << p.temp << rpmOut << pwmOut << end_separator;
  return os;
}

istream &fan::operator>>(istream &is, fan::Point &p) {
  using namespace fancon::serialization_constants::point;
  string in;
  is >> std::skipws >> in;
  if (in.empty())
    return is;
//  std::remove_if(in.begin(), in.end(), [](auto &c) { return isspace(c); });

  auto notDigit = [](const char &c) { return !std::isdigit(c); };
  InputValue tempVal(in, temp_separator, notDigit);
  InputValue pwmVal(in, pwm_separator, notDigit);
  InputValue rpmVal(in, rpm_separator, notDigit);

  // Must contain temp, and either a rpm or pwm value
  if (!tempVal.found || (!rpmVal.found & !pwmVal.found)) {
    LOG(llvl::error) << "Invalid fan config: " << in;
    return is;
  }

  if (tempVal.found)
    tempVal.setIfValid(p.temp);

  if (pwmVal.found)
    pwmVal.setIfValid(p.pwm);
  else if (rpmVal.found) {
    rpmVal.setIfValid(p.rpm);
    if (rpmVal.end != in.end())
      p.is_rpm_percent = (std::tolower(*rpmVal.end) == percent);
  }

  return is;
}

ostream &fan::operator<<(ostream &os, const fan::Config &c) {
  for (const auto &p : c.points)
    os << p;

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