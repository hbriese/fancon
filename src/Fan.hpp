#ifndef FANCON_FAN_HPP
#define FANCON_FAN_HPP

#include "Config.hpp"
#include "FanInterface.hpp"
#include "UID.hpp"
#include "Util.hpp"
#include <algorithm> // lower_bound
#include <boost/filesystem.hpp>
#include <chrono>  // system_clock::now, duration
#include <thread>  // this_thread::sleep
#include <utility> // pair, make_pair

using fancon::FanInterface;
using fancon::Util::read;
using fancon::Util::write;
using fancon::UID;

namespace fancon {
class Fan : public FanInterface {
public:
  explicit Fan(const UID &fanUID, const fan::Config &conf = fan::Config(),
               bool dynamic = true);
  ~Fan() override { writeEnableMode(driver_enable_mode); }

  pwm_t readPWM() override { return read<pwm_t>(p.pwm); }
  void writePWM(const pwm_t &pwm) override;

  rpm_t readRPM() override { return read<rpm_t>(p.rpm); }

  bool writeEnableMode(const enable_mode_t &mode) override;
  enable_mode_t readEnableMode();

private:
  struct Paths {
    Paths(const UID &uid, const string &hwID);
    string pwm, rpm, enable_pf;
  } const p;
};
} // end fancon

#endif // FANCON_FAN_HPP
