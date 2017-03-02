#include "TempSensorParent.hpp"

using namespace fancon;

TempSensorParent::TempSensorParent(TempSensorParent &&other) : ts_path(other.ts_path), temp(other.temp) {
  for (auto &fp : fans)
    fans.emplace_back(move(fp));
  fans.clear();
}

bool TempSensorParent::update() {
  int newTemp = read<int>(ts_path) / 1000;   // 3 decimal places given by sysfs

  if (newTemp != temp) {
    temp = newTemp;
    return true;
  }
  return false;
}
bool TempSensorParent::operator==(const UID &uid) const {
  auto endIt = (find(ts_path.rbegin(), ts_path.rend(), '_') + 1).base();
  return uid.getBasePath() == string(ts_path.begin(), endIt);
}