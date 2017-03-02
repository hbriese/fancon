#ifndef FANCON_TEMPSENSORPARENT_HPP
#define FANCON_TEMPSENSORPARENT_HPP

#include <memory>     // unique_ptr
#include "UID.hpp"
#include "FanInterface.hpp"

using std::unique_ptr;
using fancon::UID;
using fancon::FanInterface;

namespace fancon {
class TempSensorParent {
public:
  TempSensorParent(UID tsUID, int temp = 0)
      : ts_path(tsUID.getBasePath() + "_input"), temp(temp) {}
  TempSensorParent(TempSensorParent &&other);

  bool operator==(const UID &uid) const;

  constexpr static const char *path_pf = "temp";

  string ts_path;
  vector<unique_ptr<FanInterface>> fans;
  int temp;

  bool update();
};
}

#endif //FANCON_TEMPSENSORPARENT_HPP
