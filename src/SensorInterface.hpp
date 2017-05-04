#ifndef FANCON_SENSORINTERFACE_HPP
#define FANCON_SENSORINTERFACE_HPP

#include "Config.hpp"
#include "UID.hpp"

using fancon::UID;
using fancon::temp_t;

namespace fancon {
struct SensorInterface {
  explicit SensorInterface(temp_t temp = {}, bool update = true)
      : temp(temp), update(update) {}

  temp_t temp;
  bool update;

  virtual bool operator==(const UID &other) const = 0;

  virtual temp_t read() const = 0;
  void refresh();
};

struct Sensor : public SensorInterface {
  explicit Sensor(const UID &uid);

  string input_path;

  bool operator==(const UID &other) const override;

  temp_t read() const override { return Util::read<temp_t>(input_path) / 1000; }
};
}

#endif // FANCON_SENSORINTERFACE_HPP
