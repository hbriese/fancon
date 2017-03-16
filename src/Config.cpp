#include "Config.hpp"

using namespace fancon;

ostream &fancon::operator<<(ostream &os, const fancon::SensorControllerConfig &c) {
  os << c.update_bsep << c.update_time_s << " " << c.threads_bsep << c.threads
     << " " << c.dynamic_bsep << ((c.dynamic) ? "true" : "false");
  return os;
}

istream &fancon::operator>>(istream &is, fancon::SensorControllerConfig &c) {
  std::istreambuf_iterator<char> eos;
  string in(std::istreambuf_iterator<char>(is), eos);


  // order of variables does not matter
  auto dynamicBegIt = search(in.begin(), in.end(), c.dynamic_bsep.begin(), c.dynamic_bsep.end());
  if (validIters(in.end(), {dynamicBegIt}))
    dynamicBegIt += c.dynamic_bsep.size();
  auto dynamicEndIt = find_if(dynamicBegIt, in.end(), [](const char &ch) { return !std::isalpha(ch); });

  auto updateBegIt = search(in.begin(), in.end(), c.update_bsep.begin(), c.update_bsep.end());
  if (validIters(in.end(), {updateBegIt}))
    updateBegIt += c.update_bsep.size();
  auto updateEndIt = find_if(updateBegIt, in.end(), [](const char &ch) { return !std::isdigit(ch); });

  auto threadsBegIt = search(in.begin(), in.end(), c.threads_bsep.begin(), c.threads_bsep.end());
  if (validIters(in.end(), {threadsBegIt}))
    threadsBegIt += c.threads_bsep.size();
  auto threadsEndIt = find_if(threadsBegIt, in.end(), [](const char &ch) { return !std::isdigit(ch); });

  bool dynamicFound = validIters(in.end(), {dynamicBegIt});
  bool updateFound = validIters(in.end(), {updateBegIt});
  bool threadsFound = validIters(in.end(), {threadsBegIt});

  // fail if
  if ((!dynamicFound & !updateFound & !threadsFound) ||
      ((dynamicEndIt == in.end()) & (updateEndIt == in.end()) & (threadsEndIt == in.end()))) {
    c.update_time_s = 0;
    LOG(llvl::debug) << "Invalid entry: " << in;
    return is;
  }

  if (dynamicFound) {
    string dynamicStr(dynamicBegIt, dynamicEndIt);
    c.dynamic = (dynamicStr != "false" || dynamicStr != "0");
  }

  if (updateFound) {
    string updateStr(updateBegIt, updateEndIt);
    c.update_time_s = (isNum(updateStr)) ? (uint) std::stoul(updateStr) : c.update_time_s;
  }

  if (threadsFound) {
    string threadsStr(threadsBegIt, threadsEndIt);
    uint res = (isNum(threadsStr)) ? (uint) std::stoul(threadsStr) : c.threads;
    c.threads = (res <= std::thread::hardware_concurrency() && res > 0) ? res : c.threads;
  }

  return is;
}

Point &fancon::Point::operator=(const Point &other) {
  temp = other.temp;
  rpm = other.rpm;
  pwm = other.pwm;

  return *this;
}

ostream &fancon::operator<<(ostream &os, const Point &p) {
  string rpm_out = (p.rpm != -1) ? string() + p.rpm_bsep + to_string(p.rpm) : string();
  string pwm_out = (p.pwm != -1) ? string() + p.pwm_bsep + to_string(p.pwm) : string();
  os << p.temp_bsep << p.temp << rpm_out << pwm_out << p.esep;
  return os;
}

istream &fancon::operator>>(istream &is, Point &p) {
  string in;
  is >> in;
  if (in.empty())
    return is;
  std::remove_if(in.begin(), in.end(), [](auto &c) { return isspace(c); });

  string::iterator rpmSepIt, pwmSepIt;
  bool rpmSepFound = (rpmSepIt = find(in.begin(), in.end(), p.rpm_bsep)) != in.end();
  bool pwmSepFound = (pwmSepIt = find(in.begin(), in.end(), p.pwm_bsep)) != in.end();

  auto tempBegIt = find(in.begin(), in.end(), p.temp_bsep) + 1;
  auto tempAbsEndIt = (rpmSepFound) ? rpmSepIt : pwmSepIt;
  string::iterator tempEndIt = find_if(tempBegIt, tempAbsEndIt, [](const char &c) { return !isdigit(c); });

  auto rpmBegIt = rpmSepIt + 1;
  auto rpmEndIt = find(rpmBegIt, in.end(), (pwmSepFound) ? p.pwm_bsep : p.esep);

  auto pwmBegIt = pwmSepIt + 1;
  auto pwmEndIt = find(pwmBegIt, in.end(), p.esep);

  auto tempFound = validIters(in.end(), {tempBegIt, tempEndIt, tempAbsEndIt});
  bool rpmFound = validIters(in.end(), {rpmBegIt, rpmEndIt});
  bool pwmFound = validIters(in.end(), {pwmBegIt, pwmEndIt});

  // must contain temp, and either a rpm or pwm value
  if (!tempFound || (!rpmFound & !pwmFound)) {
    LOG(llvl::error) << "Invalid fan config: " << in;
    return is;
  }

  string tempStr = (tempFound) ? string(tempBegIt, tempEndIt) : string();
  p.temp = (isNum(tempStr)) ? stoi(tempStr) : 0;
  // check for fahrenheit, convert to celsius if found
  if (std::tolower(*tempEndIt) == p.fahrenheit)
    p.temp = static_cast<int>((p.temp - 32) / 1.8);

  string rpmStr = (rpmFound) ? string(rpmBegIt, rpmEndIt) : string();
  p.rpm = (isNum(rpmStr)) ? stoi(rpmStr) : -1;

  string pwmStr = (pwmFound) ? string(pwmBegIt, pwmEndIt) : string();
  p.pwm = (isNum(pwmStr)) ? stoi(pwmStr) : -1;

  return is;
}

ostream &fancon::operator<<(ostream &os, const FanConfig &c) {
  for (auto p : c.points)
    os << p;

  return os;
}

istream &fancon::operator>>(istream &is, FanConfig &c) {
  while (!is.eof()) {
    Point p;
    is >> p;
    if (p.valid())
      c.points.push_back(p);
  }

  return is;
}