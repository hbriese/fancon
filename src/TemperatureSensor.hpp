#ifndef fancon_TEMPERATURESENSOR_HPP
#define fancon_TEMPERATURESENSOR_HPP

#include "Util.hpp"

using fancon::Util::read;

namespace fancon {
class TemperatureSensor {
public:
  constexpr static const char *path_pf = "temp";

  static int getTemp(const string &path) {
    return read<int>(path + "_input") / 1000;  // 3 decimal places given by sysfs
  }
};
}

#endif //fancon_TEMPERATURESENSOR_HPP
