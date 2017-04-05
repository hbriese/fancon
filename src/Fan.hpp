#ifndef FANCON_FAN_HPP
#define FANCON_FAN_HPP

#include <algorithm>    // lower_bound
#include <array>
#include <chrono>       // system_clock::now, duration
#include <thread>       // this_thread::sleep
#include <utility>      // pair, make_pair
#include <boost/filesystem.hpp>
#include "FanInterface.hpp"
#include "Util.hpp"
#include "UID.hpp"
#include "Config.hpp"

using fancon::FanInterface;
using fancon::Util::read;
using fancon::Util::write;
using fancon::UID;

namespace fancon {
class Fan : public FanInterface {
public:
  Fan(const UID &fanUID, const fan::Config &conf = fan::Config(), bool dynamic = true);
  ~Fan() { writeEnableMode(driver_enable_mode); }

  pwm_t readPWM() { return read<pwm_t>(p.pwm); }
  void writePWM(const pwm_t &pwm) {
    // Attempt to recover control of the device if the write fails
    if (!write(p.pwm, pwm))
      FanInterface::recoverControl(p.pwm);
  }
  rpm_t readRPM() { return read<rpm_t>(p.rpm); }
  bool writeEnableMode(const enable_mode_t &mode) { return write(p.enable_pf, hw_id_str, mode, DeviceType::fan, true); }
  rpm_t readEnableMode() { return read<enable_mode_t>(p.enable_pf, hw_id_str, DeviceType::fan, true); }

private:
  struct Paths {
    Paths(const UID &uid, const string &hwID);
    string pwm, rpm, enable_pf;
  } const p;
};
}   // end fancon

#endif //FANCON_FAN_HPP
