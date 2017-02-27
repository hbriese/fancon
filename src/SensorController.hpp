#ifndef fancon_SENSORCONTROLLER_HPP
#define fancon_SENSORCONTROLLER_HPP

#include <algorithm>    // search, find_if
#include <iostream>     // skipws, endl
#include <utility>      // move, pair
#include <memory>       // unique_ptr, shared_ptr, make_shared
#include <sstream>      // stringstream, ostream
#include <vector>
#include <sensors/sensors.h>
#include <pstreams/pstream.h>   // pstream
#include "Util.hpp"
#include "UID.hpp"
#include "Config.hpp"
#include "Fan.hpp"
#include "FanNVIDIA.hpp"
#include "TemperatureSensor.hpp"

using std::endl;
using std::search;
using std::string;
using std::skipws;
using std::find_if;
using std::unique_ptr;
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
using fancon::Util::getLastNum;
using fancon::Util::validIter;

using SensorChip = unique_ptr<const sensors_chip_name *>;

namespace fancon {
class TSParent;
class SensorControllerConfig;

class SensorController {
public:
  SensorController(bool debug, uint nThreads = 0);
  ~SensorController();

  fancon::SensorControllerConfig conf;

  bool skipLine(const string &line);
  inline vector<UID> getFans() { return getUIDs(Fan::path_pf); }
  vector<UID> getNvidiaFans();
  vector<UID> getFansAll();
  inline vector<UID> getSensors() { return getUIDs(TemperatureSensor::path_pf); }

  void writeConf(const string &path);
  vector<unique_ptr<fancon::TSParent>> readConf(const string &path);

private:
  vector<SensorChip> sensor_chips;

  vector<SensorChip> getSensorChips();
  vector<UID> getUIDs(const char *devicePathPostfix);

  bool checkNvidiaSupport();
  static void enableNvidiaFanControlCoolbit();
};

class TSParent {
public:
  TSParent(UID tsUID, int temp = 0) : ts_uid(tsUID), temp(temp) {}
  TSParent(TSParent &&other);

  UID ts_uid;
  vector<unique_ptr<Fan>> fans;
  vector<unique_ptr<FanNVIDIA>> fansNVIDIA;
  int temp;

  bool update();
};
}

#endif //fancon_SENSORCONTROLLER_HPP
