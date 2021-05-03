#include "Sensor.hpp"

fc::Sensor::Sensor(string label_) : label(move(label_)) {}

Temp fc::Sensor::get_average_temp() {
  std::scoped_lock lock(read_mutex);
  if (fresh())
    return last_avg_temp;

  const auto temp = read();
  if (temp) {
    if (!temp_history.empty()) {
      temp_history[temp_history_i] = *temp;
      temp_history_i = (temp_history_i < temp_history.size()) ? temp_history_i + 1 : 0;
    } else {
      temp_history.resize(fc::temp_averaging_intervals, *temp);
    }
  } else {
    LOG(llvl::error) << *this << ": failed to read";
  }

  last_read_time = chrono::high_resolution_clock::now();
  last_avg_temp = std::accumulate(temp_history.begin(), temp_history.end(), 0) / temp_history.size();

  LOG(llvl::trace) << *this << ": " << last_avg_temp << "°" << fc::log::flush;

  return last_avg_temp;
}

void fc::Sensor::from(const fc_pb::Sensor &s) { label = s.label(); }

void fc::Sensor::to(fc_pb::Sensor &s) const { s.set_label(label); }

bool fc::Sensor::deep_equal(const Sensor &other) const {
  fc_pb::Sensor s, sother;
  to(s);
  other.to(sother);
  return Util::deep_equal(s, sother);
}

bool fc::Sensor::fresh() const {
  const auto dur = chrono::duration_cast<milliseconds>(chrono::high_resolution_clock::now() - last_read_time);
  return dur <= milliseconds(200);
}

std::ostream &fc::operator<<(std::ostream &os, const fc::Sensor &s) {
  return os << s.label;
}
