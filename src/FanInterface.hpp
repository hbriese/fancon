#ifndef FANCON_FANINTERFACE_HPP
#define FANCON_FANINTERFACE_HPP

#include <cmath>
#include <regex>

#include "SensorInterface.hpp"
#include "Util.hpp"
#include "proto/DevicesSpec.pb.h"

using std::abs;
using std::min;
using std::next;
using std::stoi;

using Pwm = int;
using Rpm = int;
using Percent = uint;
using Rpm_to_Pwm_Map = std::map<Rpm, Pwm>;
using Pwm_to_Rpm_Map = std::map<Pwm, Rpm>;
using Temp_to_Rpm_Map = std::map<Temp, Rpm>;

namespace fc {
extern bool dynamic;
extern uint smoothing_intervals;
extern uint top_stickiness_intervals;
enum class ControllerState;
extern ControllerState controller_state;

const Pwm pwm_min_abs = 0, pwm_max_abs = 255;

class FanInterface {
public:
  FanInterface() = default;
  explicit FanInterface(string label_);
  virtual ~FanInterface() = default;

  string label;
  shared_ptr<fc::SensorInterface> sensor;
  Rpm_to_Pwm_Map rpm_to_pwm;
  Temp_to_Rpm_Map temp_to_rpm;
  Pwm start_pwm = 0;
  milliseconds interval{0};
  bool ignore{false};

  bool update();
  virtual void test(shared_ptr<double> completed);
  bool tested() const;
  bool configured() const;

  virtual bool enable_control() const = 0;
  virtual bool disable_control() const = 0;

  virtual void from(const fc_pb::Fan &f, const SensorMap &sensor_map);
  virtual void to(fc_pb::Fan &f) const = 0;
  virtual bool valid() const = 0;
  virtual string uid() const = 0;

  friend std::ostream &operator<<(std::ostream &os, const FanInterface &f);

protected:
  struct {
    bool just_started{true};
    int rem_intervals{0};
    Rpm targeted_rpm{0};
    int top_stickiness_rem_intervals{0};
  } smoothing;

  virtual bool set_pwm(const Pwm pwm) const = 0;
  virtual Pwm get_pwm() const = 0;
  bool set_rpm(Rpm rpm);
  virtual Rpm get_rpm() const = 0;
  bool recover_control() const;
  Rpm smooth_rpm(const Rpm rpm);
  void sleep_for_interval() const;

  optional<Rpm> set_stabilised_pwm(const Pwm pwm) const;
  bool set_pwm_test() const;
  void test_stopped(Pwm_to_Rpm_Map &pwm_to_rpm);
  void test_start(Pwm_to_Rpm_Map &pwm_to_rpm);
  void test_interval(Pwm_to_Rpm_Map &pwm_to_rpm);
  void test_running_min(Pwm_to_Rpm_Map &pwm_to_rpm);
  void test_mapping(Pwm_to_Rpm_Map &pwm_to_rpm);

  Percent rpm_to_percent(const Rpm rpm) const;
  Rpm percent_to_rpm(Percent percent) const;
  Rpm pwm_to_rpm(Pwm pwm) const;
  void temp_to_rpm_from(const string &src);
  void rpm_to_pwm_from(const string &src);
  void rpm_to_pwm_from(const Pwm_to_Rpm_Map &pwm_to_rpm);
};

std::ostream &operator<<(std::ostream &os, const fc::FanInterface &f);
} // namespace fc

#endif // FANCON_FANINTERFACE_HPP
