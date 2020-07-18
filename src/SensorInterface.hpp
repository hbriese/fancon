#ifndef FANCON_SENSORINTERFACE_HPP
#define FANCON_SENSORINTERFACE_HPP

#include <mutex>
#include <numeric>

#include "Util.hpp"
#include "proto/DevicesSpec.pb.h"

using Temp = int;

namespace fc {
extern uint temp_averaging_intervals;

class SensorInterface {
public:
  SensorInterface() = default;
  explicit SensorInterface(string label_);
  virtual ~SensorInterface() = default;

  string label;
  bool ignore{false};

  Temp get_average_temp();
  virtual optional<Temp> min_temp() const { return nullopt; }
  virtual optional<Temp> max_temp() const { return nullopt; }

  virtual void from(const fc_pb::Sensor &s);
  virtual void to(fc_pb::Sensor &s) const;
  virtual bool valid() const = 0;
  virtual string hw_id() const = 0;

  friend std::ostream &operator<<(std::ostream &os, const SensorInterface &s);

protected:
  std::mutex read_mutex;
  chrono::high_resolution_clock::time_point last_read_time;
  vector<Temp> temp_history;
  size_t temp_history_i = 0;
  Temp last_avg_temp = 0;

  virtual Temp read() const = 0;
  bool fresh() const;
};

std::ostream &operator<<(std::ostream &os, const SensorInterface &s);
} // namespace fc

using SensorMap = std::unordered_map<string, shared_ptr<fc::SensorInterface>>;

#endif // FANCON_SENSORINTERFACE_HPP
