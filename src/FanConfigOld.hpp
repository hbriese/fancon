#ifndef FANCON_FANCONFIG_HPP
#define FANCON_FANCONFIG_HPP

#include <algorithm>    // search, remove_if
#include <cctype>       // isspace
#include <exception>    // runtime_error
#include <iostream>     // skipws, cin
#include <string>       // to_string, stoi, stod
#include <sstream>      // ostream, istream
#include <vector>

using std::string;
using std::search;
using std::skipws;
using std::stoi;
using std::ostream;
using std::istream;
using std::to_string;
using std::vector;

namespace fancon {
class FanConfigOld {
public:
  FanConfigOld(istream &is) { is >> *this; }

  int pwm_min,
      pwm_max;
  int rpm_min,
      rpm_max;
  int pwm_start;
  int rpm_floor;      // lowest fan pwm before rpm = 0
  double slope;       // i.e. rpm-per-pwm

  vector<Point> points;

  /* FORMAT:
   * {rpm_min(rpm_max)/pwm_min(pwm_max);pwm_start,rpm_floor,slope}
   */

  friend ostream &operator<<(ostream &os, const FanConfigOld &c) {
    os << c.pre_rpm_min_s << to_string(c.rpm_min) << c.rpm_min_s << to_string(c.rpm_max) << c.rpm_max_s
       << to_string(c.pwm_min) << c.pwm_min_s << to_string(c.pwm_max) << c.pwm_max_s
       << to_string(c.pwm_start) << c.pwm_start_s << to_string(c.rpm_floor) << c.rpm_floor_s
       << to_string(c.slope) << c.slope_s << " ";

    for (auto p : c.points)
      os << p;

    return os;
  }

  friend istream &operator>>(istream &is, FanConfigOld &c) {
    string in;
    is >> skipws >> in;
    std::remove_if(in.begin(), in.end(), [](auto ch) { return isspace(ch, std::cin.getloc()); });

    auto pre_rpm_min_s_it = search(in.begin(), in.end(), c.pre_rpm_min_s.begin(), c.pre_rpm_min_s.end());
    auto rpm_min_s_it = search(pre_rpm_min_s_it, in.end(), c.rpm_min_s.begin(), c.rpm_min_s.end());
    auto rpm_max_s_it = search(rpm_min_s_it, in.end(), c.rpm_max_s.begin(), c.rpm_min_s.end());
    auto pwm_min_s_it = search(rpm_max_s_it, in.end(), c.pwm_min_s.begin(), c.pwm_min_s.end());
    auto pwm_max_s_it = search(pwm_min_s_it, in.end(), c.pwm_max_s.begin(), c.pwm_max_s.end());
    auto pwm_start_s_it = search(pwm_max_s_it, in.end(), c.pwm_start_s.begin(), c.pwm_start_s.end());
    auto rpm_floor_s_it = search(pwm_start_s_it, in.end(), c.rpm_floor_s.begin(), c.rpm_floor_s.end());
    auto slope_s_it = search(rpm_floor_s_it, in.end(), c.slope_s.begin(), c.slope_s.end());

    if (pre_rpm_min_s_it == in.end() || rpm_min_s_it == in.end() || rpm_max_s_it == in.end() ||
        pwm_min_s_it == in.end() || pwm_max_s_it == in.end() || pwm_start_s_it == in.end() ||
        rpm_floor_s_it == in.end() || slope_s_it == in.end())
      throw std::runtime_error(in.c_str());

    c.rpm_min = stoi(string(pre_rpm_min_s_it + 1, rpm_min_s_it - 1));
    c.rpm_max = stoi(string(rpm_min_s_it + 1, rpm_max_s_it - 1));
    constexpr auto rpm_max_s_len = c.rpm_max_s.length();
    c.pwm_min = stoi(string(rpm_max_s_it + rpm_max_s_len, pwm_min_s_it - 1));
    c.pwm_max = stoi(string(pwm_min_s_it + 1, pwm_max_s_it - 1));
    constexpr auto pwm_max_s_len = c.pwm_max_s.length();
    c.pwm_start = stoi(string(pwm_max_s_it + pwm_max_s_len, pwm_start_s_it - 1));
    c.rpm_floor = stoi(string(pwm_start_s_it + 1, rpm_floor_s_it - 1));
    c.slope = std::stod(string(rpm_floor_s_it + 1, slope_s_it - 1));

    std::stringstream ss(string(slope_s_it + 1, in.end() - 1));

    Point p;
    while (!(ss >> p).eof())
      c.points.push_back(p);

    return is;
  }

private:
  // seperators
  const string pre_rpm_min_s = "{",
      rpm_min_s = "(",
      rpm_max_s = ")/",
      pwm_min_s = "(",
      pwm_max_s = ");",
      pwm_start_s = ",",
      rpm_floor_s = ",",
      slope_s = "}";

  class Point {
  public:
    Point(int temp = 0, int rpm = 0)
        : temp(temp), rpm(rpm) {}

    int temp;
    int rpm;

    /* FORMAT:
     * [temp,RPM]
     */

    friend ostream &operator<<(ostream &os, const Point &p) {
      os << p.pre_temp_s << to_string(p.temp) << p.temp_s << to_string(p.rpm) << p.rpm_s;
      return os;
    }

    friend istream &operator>>(istream &is, Point &p) {
      string in;
      is >> skipws >> in;
      std::remove_if(in.begin(), in.end(), [](auto c) { return isspace(c, std::cin.getloc()); });

      auto pre_temp_s_it = search(in.begin(), in.end(), p.pre_temp_s.begin(), p.pre_temp_s.end());
      auto temp_s_it = search(pre_temp_s_it, in.end(), p.temp_s.begin(), p.temp_s.end());
      auto rpm_s_it = search(rpm_s_it, in.end(), p.rpm_s.begin(), p.rpm_s.end());

      if (pre_temp_s_it == in.end() || temp_s_it == in.end() || rpm_s_it == in.end())
        throw std::runtime_error(in.c_str());

      p.temp = stoi(string(pre_temp_s_it + 1, temp_s_it - 1));
      p.rpm = stoi(string(temp_s_it + 1, rpm_s_it - 1));

      return is;
    }

  private:
    const string pre_temp_s = "[",
        temp_s = ",",
        rpm_s = "]";
  };
};
}

#endif //FANCON_FANCONFIG_HPP