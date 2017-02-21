#ifndef fancon_TEMPERATURESENSOR_HPP
#define fancon_TEMPERATURESENSOR_HPP

#include "Util.hpp"
#include "UID.hpp"

using std::to_string;
using fancon::Util::read;

namespace fancon {
class TemperatureSensor {
public:
  static int getTemp(const string &path) {
    return read<int>(path + "_input", 0) / 1000;  // 3 decimal places given by sysfs
  }

  constexpr static const char *path_pf = "temp";
};
}

#endif //fancon_TEMPERATURESENSOR_HPP
