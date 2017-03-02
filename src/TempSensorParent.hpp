#ifndef FANCON_TEMPSENSORPARENT_HPP
#define FANCON_TEMPSENSORPARENT_HPP

#include <memory>
#include "UID.hpp"
#include "Fan.hpp"
#include "FanNV.hpp"

using std::unique_ptr;
using fancon::UID;
using fancon::Fan;
using fancon::FanNV;

namespace fancon {
class TempSensorParent {
public:
  TempSensorParent(UID tsUID, int temp = 0)
      : ts_path(tsUID.getBasePath() + "_input"), temp(temp) {}
  TempSensorParent(TempSensorParent &&other);

  bool operator==(const UID &uid) const;

  constexpr static const char *path_pf = "temp";

  string ts_path;
  vector<unique_ptr<Fan>> fans;
  vector<unique_ptr<FanNV>> fansNVIDIA;
  int temp;

  bool update();
};
}

#endif //FANCON_TEMPSENSORPARENT_HPP
