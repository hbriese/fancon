#include "Config.hpp"

using namespace fancon;

ostream &fancon::operator<<(ostream &os, const fancon::SensorControllerConfig &c) {
  os << c.updateBegSep << c.update_time_s << " " << c.threadsBegSep << c.threads
     << " " << c.dynamicBegSep << ((c.dynamic) ? "true" : "false");
  return os;
}

istream &fancon::operator>>(istream &is, fancon::SensorControllerConfig &c) {
  std::istreambuf_iterator<char> eos;
  string in(std::istreambuf_iterator<char>(is), eos);


  // order of variables does not matter
  auto dynamicBegIt = search(in.begin(), in.end(), c.dynamicBegSep.begin(), c.dynamicBegSep.end());
  if (validIter(in.end(), {dynamicBegIt}))
    dynamicBegIt += c.dynamicBegSep.size();
  auto dynamicEndIt = find_if(dynamicBegIt, in.end(), [](const char &ch) { return !std::isalpha(ch); });

  auto updateBegIt = search(in.begin(), in.end(), c.updateBegSep.begin(), c.updateBegSep.end());
  if (validIter(in.end(), {updateBegIt}))
    updateBegIt += c.updateBegSep.size();
  auto updateEndIt = find_if(updateBegIt, in.end(), [](const char &ch) { return !std::isdigit(ch); });

  auto threadsBegIt = search(in.begin(), in.end(), c.threadsBegSep.begin(), c.threadsBegSep.end());
  if (validIter(in.end(), {threadsBegIt}))
    threadsBegIt += c.threadsBegSep.size();
  auto threadsEndIt = find_if(threadsBegIt, in.end(), [](const char &ch) { return !std::isdigit(ch); });

  bool dynamicFound = validIter(in.end(), {dynamicBegIt});
  bool updateFound = validIter(in.end(), {updateBegIt});
  bool threadsFound = validIter(in.end(), {threadsBegIt});

  // fail if
  if ((!dynamicFound & !updateFound & !threadsFound) ||
      ((dynamicEndIt == in.end()) & (updateEndIt == in.end()) & (threadsEndIt == in.end()))) {
    c.update_time_s = 0;
    LOG(severity_level::debug) << "Invalid entry: " << in;
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
  string rpm_out = (p.rpm != -1) ? string() + p.rpmBegSep + to_string(p.rpm) : string();
  string pwm_out = (p.pwm != -1) ? string() + p.pwmBegSep + to_string(p.pwm) : string();
  os << p.tempBegSep << p.temp << rpm_out << pwm_out << p.endSep;
  return os;
}

istream &fancon::operator>>(istream &is, Point &p) {
  string in;
  is >> in;
  std::remove_if(in.begin(), in.end(), [](auto &c) { return isspace(c); });

  string::iterator rpmSepIt, pwmSepIt;
  bool rpmSepFound = (rpmSepIt = find(in.begin(), in.end(), p.rpmBegSep)) != in.end();
  bool pwmSepFound = (pwmSepIt = find(in.begin(), in.end(), p.pwmBegSep)) != in.end();

  auto tempBegIt = find(in.begin(), in.end(), p.tempBegSep) + 1;
  auto tempAbsEndIt = (rpmSepFound) ? rpmSepIt : pwmSepIt;
  string::iterator tempEndIt = find_if(tempBegIt, tempAbsEndIt, [](const char &c) { return !isdigit(c); });

  auto rpmBegIt = rpmSepIt + 1;
  auto rpmEndIt = find(rpmBegIt, in.end(), (pwmSepFound) ? p.pwmBegSep : p.endSep);

  auto pwmBegIt = pwmSepIt + 1;
  auto pwmEndIt = find(pwmBegIt, in.end(), p.endSep);

  auto tempFound = validIter(in.end(), {tempBegIt, tempEndIt, tempAbsEndIt});
  bool rpmFound = validIter(in.end(), {rpmBegIt, rpmEndIt});
  bool pwmFound = validIter(in.end(), {pwmBegIt, pwmEndIt});

  // must contain temp, and either a rpm or pwm value
  if (!tempFound || (!rpmFound & !pwmFound))
    LOG(severity_level::error) << "Invalid fan config: " << in;

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