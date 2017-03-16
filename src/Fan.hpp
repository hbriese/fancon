#ifndef fancon_FAN_HPP
#define fancon_FAN_HPP

#include <algorithm>    // lower_bound
#include <array>
#include <chrono>       // system_clock::now, duration
#include <functional>
#include <thread>       // this_thread::sleep
#include <utility>      // pair, make_pair
#include <boost/filesystem.hpp>
#include "FanInterface.hpp"
#include "Util.hpp"
#include "UID.hpp"
#include "Config.hpp"

namespace bfs = boost::filesystem;

using std::next;
using std::prev;
using std::stringstream;
using std::function;
using boost::filesystem::path;
using boost::filesystem::exists;
using fancon::FanInterface;
using fancon::Util::read;
using fancon::Util::write;
using fancon::UID;
using fancon::FanConfig;

namespace fancon {
class Fan : public FanInterface {
public:
  Fan(const UID &fanUID, const FanConfig &conf = FanConfig(), bool dynamic = true);
  ~Fan() { writeEnableMode(driver_enable_mode); }

  constexpr static const char *path_pf = "fan";

  int readPWM() { return read<int>(pwm_p); }
  void writePWM(int pwm) { write(pwm_p, pwm); }
  int readRPM() { return read<int>(rpm_p); }
  void writeEnableMode(int mode) { write(enable_pf, hw_id_str, mode, DeviceType::FAN, true); }
  int readEnableMode() { return read<int>(enable_pf, hw_id_str, DeviceType::FAN, true); }

  int testPWM(int rpm);    // TODO: remove

  using FanInterface::writeTestResult;

private:
  string pwm_p, rpm_p;
  string enable_pf;
};
}   // fancon

#endif //fancon_FAN_HPP
