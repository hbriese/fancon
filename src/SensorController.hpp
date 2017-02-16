#ifndef FANCON_SENSORCONTROLLER_HPP
#define FANCON_SENSORCONTROLLER_HPP

#include <algorithm>    // search, find_if
#include <iostream>     // skipws, endl
#include <utility>      // pair
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
using std::istringstream;
using std::ostringstream;
using std::pair;
using std::vector;
using fancon::UID;
using fancon::Fan;
using fancon::SensorControllerConfig;
using fancon::Config;
using fancon::TemperatureSensor;
using fancon::Util::DaemonState;
using fancon::Util::getLastNum;

using SensorChip = shared_ptr<const sensors_chip_name *>;

namespace fancon {
struct TSParent;
class SensorControllerConfig;

class SensorController {
public:
  SensorController(bool debug = false);
  ~SensorController();

  vector<UID> getUIDs(const char *device_path_postfix);
  void writeConf(const string &path);
  vector<fancon::TSParent> readConf(const string &path);

  // TODO: make state& const?
  void run(vector<TSParent>::iterator first, vector<TSParent>::iterator last, DaemonState &state) const;

private:
  fancon::SensorControllerConfig conf;
  vector<SensorChip> sensor_chips;

  vector<SensorChip> getSensorChips();
};

struct TSParent {
  TSParent(const UID ts_uid_, Fan &f, int temp = 0)
      : ts_uid(ts_uid_), temp(temp) { fans.push_back(f); }

  const UID ts_uid;
  vector<Fan> fans;
  int temp;

  bool update();
};
}

#endif //FANCON_SENSORCONTROLLER_HPP
