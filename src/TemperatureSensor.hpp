#ifndef FANCTL_TEMPERATURESENSOR_HPP
#define FANCTL_TEMPERATURESENSOR_HPP

#include "Util.hpp"
#include "UID.hpp"

using std::to_string;
using fanctl::Util::read;

namespace fanctl {
class TemperatureSensor {
public:
  static int getTemp(const string &path) {
    return read<int>(path + "_input") / 1000;  // 3 decimal places given by sysfs
  }

  constexpr static const char *path_pf = "temp";
};
}

#endif //fanctl_TEMPERATURESENSOR_HPP
