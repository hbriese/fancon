#ifndef FANCON_SENSORINTERFACE_HPP
#define FANCON_SENSORINTERFACE_HPP

#include "UID.hpp"

using fancon::UID;

namespace fancon {
struct SensorInterface {
  SensorInterface(int temp = 0) : temp(temp) {}

  int temp;
  bool update = true;

  virtual bool operator==(const UID &other) const = 0;

  virtual int read() = 0;
  void refresh();
};

struct Sensor : public SensorInterface {
  Sensor(const UID &uid) : input_path(uid.getBasePath() + "_input") {}

  string input_path;

  bool operator==(const UID &other) const;

  int read() { return Util::read<int>(input_path) / 1000; }
};
}

#endif //FANCON_SENSORINTERFACE_HPP