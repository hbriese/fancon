#ifdef FANCON_NVIDIA_SUPPORT
#ifndef FANCON_NVIDIADEVICES_HPP
#define FANCON_NVIDIADEVICES_HPP

#include "FanInterface.hpp"
#include "NvidiaUtil.hpp"
#include "SensorInterface.hpp"

namespace fc {
class FanNV : public fc::FanInterface {
public:
  FanNV() = default;
  FanNV(string label, NVID id);
  ~FanNV() override;

  void from(const fc_pb::Fan &f, const SensorMap &sensor_map) override;
  void to(fc_pb::Fan &f) const override;
  bool valid() const override;
  string uid() const override;

  static vector<unique_ptr<fc::FanInterface>> enumerate();

private:
  NVID id{0};

  Rpm get_rpm() const override;
  Pwm get_pwm() const override;
  bool set_pwm(const Pwm pwm) const override;

  bool enable_control() const override;
  bool disable_control() const override;

  static Percent pwm_to_percent(const Pwm pwm);
  static Pwm percent_to_pwm(const Percent percent);
};

class SensorNV : public SensorInterface {
public:
  SensorNV() = default;
  SensorNV(string label, NVID id);

  Temp read() const override;

  void from(const fc_pb::Sensor &s) override;
  void to(fc_pb::Sensor &s) const override;
  bool valid() const override;
  string uid() const override;

  static SensorMap enumerate();

private:
  NVID id{0};
};
} // namespace fc

#endif // FANCON_NVIDIADEVICES_HPP
#endif // FANCON_NVIDIA_SUPPORT
