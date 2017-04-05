#ifndef FANCON_SENSORINTERFACE_HPP
#define FANCON_SENSORINTERFACE_HPP

#include "UID.hpp"
#include "Config.hpp"

using fancon::UID;
using fancon::temp_t;

namespace fancon {
struct SensorInterface {
  SensorInterface(temp_t temp = {}) : temp(temp), update(true) {}

  temp_t temp;
  bool update;

  virtual bool operator==(const UID &other) const = 0;

  virtual temp_t read() = 0;
  void refresh();
};

struct Sensor : public SensorInterface {
  Sensor(const UID &uid) : input_path(uid.getBasePath() + "_input") { input_path.shrink_to_fit(); }

  string input_path;

  bool operator==(const UID &other) const;

  temp_t read() { return Util::read<temp_t>(input_path) / 1000; }
};
}

#endif //FANCON_SENSORINTERFACE_HPP