#ifndef FANCON_TEMPSENSORPARENT_HPP
#define FANCON_TEMPSENSORPARENT_HPP

#include <memory>     // unique_ptr
#include "UID.hpp"
#include "FanInterface.hpp"

using std::unique_ptr;
using fancon::UID;
using fancon::FanInterface;

namespace fancon {
class SensorParentInterface {
public:
  SensorParentInterface(int temp = 0) : temp(temp) {}
  SensorParentInterface(SensorParentInterface &&other);

  virtual bool operator==(const UID &other) const = 0;

  vector<unique_ptr<FanInterface>> fans;
  int temp;

  virtual int read() = 0;

  bool update();
};

class SensorParent : public SensorParentInterface {
public:
  SensorParent(const UID &uid) : ts_p(uid.getBasePath() + "_input") {}
  ~SensorParent() { fans.clear(); }

  string ts_p;

  bool operator==(const UID &other) const;

  int read() { return Util::read<int>(ts_p) / 1000; }
};
}

#endif //FANCON_TEMPSENSORPARENT_HPP
