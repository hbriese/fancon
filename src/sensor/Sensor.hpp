#ifndef FANCON_SENSOR_HPP
#define FANCON_SENSOR_HPP

#include <mutex>
#include <numeric>

#include "util/Util.hpp"
#include "proto/DevicesSpec.pb.h"

using Temp = int;

namespace fc {
extern uint temp_averaging_intervals;

class Sensor {
public:
  Sensor() = default;
  explicit Sensor(string label_);
  virtual ~Sensor() = default;

  string label;
  bool ignore{false};

  Temp get_average_temp();
  virtual optional<Temp> min_temp() const { return nullopt; }
  virtual optional<Temp> max_temp() const { return nullopt; }

  virtual void from(const fc_pb::Sensor &s);
  virtual void to(fc_pb::Sensor &s) const;
  virtual bool valid() const = 0;
  virtual string hw_id() const = 0;

  bool deep_equal(const Sensor &other) const;
  friend std::ostream &operator<<(std::ostream &os, const Sensor &s);

protected:
  std::mutex read_mutex;
  chrono::high_resolution_clock::time_point last_read_time;
  vector<Temp> temp_history;
  size_t temp_history_i = 0;
  Temp last_avg_temp = 0;

  virtual optional<Temp> read() const = 0;
  bool fresh() const;
};

std::ostream &operator<<(std::ostream &os, const Sensor &s);
} // namespace fc

using SensorMap = std::unordered_map<string, shared_ptr<fc::Sensor>>;

#endif // FANCON_SENSOR_HPP
