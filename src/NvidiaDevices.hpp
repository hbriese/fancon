#ifdef FANCON_NVIDIA_SUPPORT
#ifndef FANCON_NVDEVICES_HPP
#define FANCON_NVDEVICES_HPP

#include "FanInterface.hpp"
#include "NvidiaUtil.hpp"
#include "SensorInterface.hpp"

namespace fancon {
using percent_t = int;

class FanNV : public FanInterface {
public:
  explicit FanNV(const UID &fanUID, const fan::Config &conf = fan::Config(),
                 bool dynamic = true);
  ~FanNV() override { writeEnableMode(driver_enable_mode); }

  rpm_t readRPM() override { return NV::rpm.read(hw_id); };
  pwm_t readPWM() override { return percentToPWM(NV::pwm_percent.read(hw_id)); }
  bool writePWM(const pwm_t &pwm) override;
  bool writeEnableMode(const enable_mode_t &mode) override;

private:
  static percent_t pwmToPercent(const pwm_t &pwm);
  static pwm_t percentToPWM(const percent_t &percent);
};

struct SensorNV : public SensorInterface {
  explicit SensorNV(const UID &uid) : hw_id(uid.hw_id) {}

  hwid_t hw_id;

  bool operator==(const UID &other) const override;

  temp_t read() const override { return NV::temp.read(hw_id); };
};
}

#endif // FANCON_NVDEVICES_HPP
#endif // FANCON_NVIDIA_SUPPORT
