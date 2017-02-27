#ifndef fancon_FAN_HPP
#define fancon_FAN_HPP

#include <algorithm>    // lower_bound
#include <array>
#include <chrono>       // system_clock::now, duration
#include <iterator>     // next, prev, distance
#include <functional>
#include <thread>       // this_thread::sleep
#include <utility>      // pair, make_pair
#include <boost/filesystem.hpp>
#include "FanInterface.hpp"
#include "Util.hpp"
#include "UID.hpp"
#include "Config.hpp"
#include "TemperatureSensor.hpp"

namespace bfs = boost::filesystem;

using std::prev;
using std::move;
using std::stringstream;
using std::function;
using boost::filesystem::path;
using boost::filesystem::exists;
using fancon::FanInterface;
using fancon::Util::read;
using fancon::Util::write;
using fancon::Util::log;
using fancon::UID;
using fancon::FanConfig;

namespace fancon {
class Fan : public FanInterface {
public:
  Fan(const UID &fanUID, const FanConfig &conf = FanConfig(), bool dynamic = true);
  ~Fan() { writeEnableMode(driver_enable_mode); }

  constexpr static const char *path_pf = "fan";

  inline void writePWM(int pwm) { write<int>(pwm_p, pwm); }
  inline int readRPM() { return read<int>(rpm_p); }
  inline void writeEnableMode(int mode) { write<int>(enable_pf, hwID, mode, DeviceType::FAN, true); }
  inline int readEnableMode() { return read<int>(enable_pf, hwID, DeviceType::FAN, true); }

  using FanInterface::update;
  int testPWM(int rpm);    // TODO: remove

  static FanTestResult test(const UID &fanUID);
  using FanInterface::writeTestResult;

private:
  int driver_enable_mode;

  string pwm_p, rpm_p;
  string enable_pf;
};
}   // fancon

#endif //fancon_FAN_HPP
