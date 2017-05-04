#include "SensorInterface.hpp"

using namespace fancon;

void SensorInterface::refresh() {
  temp_t newTemp{read()};

  if (newTemp != temp) {
    temp = newTemp;
    update = true;
  } else
    update = false;
}

Sensor::Sensor(const UID &uid) : input_path(uid.getBasePath() + "_input") {
  input_path.shrink_to_fit();
}

bool Sensor::operator==(const UID &other) const {
  auto basep = (find(input_path.rbegin(), input_path.rend(), '_') + 1).base();
  return other.getBasePath() == string(input_path.begin(), basep);
}
