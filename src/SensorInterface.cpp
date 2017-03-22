#include "SensorInterface.hpp"

using namespace fancon;

void SensorInterface::refresh() {
  int newTemp(read());

  if (newTemp != temp) {
    temp = newTemp;
    update = true;  // declare after updating temp, to avoid a thread updating using the wrong temp
  } else
    update = false;
}

bool Sensor::operator==(const UID &other) const {
  auto basePath = (find(input_path.rbegin(), input_path.rend(), '_') + 1).base();
  return other.getBasePath() == string(input_path.begin(), basePath);
}