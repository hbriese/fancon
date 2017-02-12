#ifndef FANCON_TEMPERATURESENSOR_HPP
#define FANCON_TEMPERATURESENSOR_HPP

#include "Util.hpp"
#include "UID.hpp"

using std::to_string;
//using fancon::Util::read;

namespace fancon {
class TemperatureSensor {
public:
  static int getTemp(const string &path) {
    string inputP = path + "_input";
    long temp = fancon::Util::read<long>(inputP);
    return fancon::Util::read<int>(inputP) / 1000;  // 3 decimal places given by driver
  }

  constexpr static const char *path_pf = "temp";
};
}

#endif //FANCON_TEMPERATURESENSOR_HPP
