#ifndef fancon_SENSORCONTROLLER_HPP
#define fancon_SENSORCONTROLLER_HPP

#include <algorithm>    // search, find_if
#include <iostream>     // skipws, endl
#include <utility>      // move, pair
#include <memory>       // unique_ptr, shared_ptr, make_shared
#include <sstream>      // stringstream, ostream
#include <vector>
#include <sensors/sensors.h>
#include "Util.hpp"
#include "Config.hpp"
#include "UID.hpp"
#include "Fan.hpp"
#include "TemperatureSensor.hpp"

using std::endl;
using std::search;
using std::string;
using std::skipws;
using std::find_if;
using std::unique_ptr;
using std::shared_ptr;
using std::make_shared;
using std::make_unique;
using std::move;
using std::istringstream;
using std::ostringstream;
using std::pair;
using std::vector;
using fancon::UID;
using fancon::Fan;
using fancon::SensorControllerConfig;
using fancon::FanConfig;
using fancon::TemperatureSensor;
using fancon::Util::DaemonState;
using fancon::Util::getLastNum;

using SensorChip = shared_ptr<const sensors_chip_name *>;

namespace fancon {
class TSParent;
class SensorControllerConfig;

class SensorController {
public:
  SensorController(bool debug, uint nThreads = 0);
  ~SensorController();

  fancon::SensorControllerConfig conf;

  vector<UID> getUIDs(const char *devicePathPostfix);

  void writeConf(const string &path);
  vector<unique_ptr<fancon::TSParent>> readConf(const string &path);

private:
  vector<SensorChip> sensor_chips;

  vector<SensorChip> getSensorChips();
};

class TSParent {
public:
  TSParent(UID tsUID, unique_ptr<Fan> fp, int temp = 0);
  TSParent(TSParent &&other);

  UID ts_uid;
  vector<unique_ptr<Fan>> fans;
  int temp;

  bool update();
};
}

#endif //fancon_SENSORCONTROLLER_HPP
