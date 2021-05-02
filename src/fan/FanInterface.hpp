#ifndef FANCON_FANINTERFACE_HPP
#define FANCON_FANINTERFACE_HPP

#include <cmath>
#include <regex>

#include "sensor/SensorInterface.hpp"
#include "util/Util.hpp"
#include "proto/DevicesSpec.pb.h"

using fc::Util::ObservableNumber;
using fc_pb::DevType;
using std::abs;
using std::min;
using std::next;
using std::regex;
using Pwm = uint;
using Rpm = uint;
using Percent = uint;
using Rpm_to_Pwm_Map = std::map<Rpm, Pwm>;
using Pwm_to_Rpm_Map = std::map<Pwm, Rpm>;
using Temp_to_Rpm_Map = std::map<Temp, Rpm>;

namespace fc {
extern milliseconds update_interval;
extern bool dynamic;
extern uint smoothing_intervals;
extern uint top_stickiness_intervals;
enum class ControllerState;
extern ControllerState controller_state;

const Pwm PWM_MIN = 0, PWM_MAX = 255;
const double STABILISED_THRESHOLD = 0.1;

Pwm clamp_pwm(Pwm pwm);

class FanInterface {
public:
  FanInterface() = default;
  explicit FanInterface(string label_);
  virtual ~FanInterface() = default;

  string label;
  bool ignore{false};

  void update();
  virtual bool test(ObservableNumber<int> &status);
  bool tested() const;
  bool try_enable();
  bool is_configured(bool log) const;

  virtual bool enable_control() = 0;
  virtual bool disable_control() = 0;
  virtual Pwm get_pwm() const = 0;
  virtual Rpm get_rpm() const = 0;
  virtual bool valid() const = 0;
  virtual string hw_id() const = 0;
  virtual DevType type() const = 0;

  virtual void from(const fc_pb::Fan &f, const SensorMap &sensor_map);
  virtual void to(fc_pb::Fan &f) const = 0;
  bool deep_equal(const FanInterface &other) const;

  friend std::ostream &operator<<(std::ostream &os, const FanInterface &f);

protected:
  shared_ptr<fc::SensorInterface> sensor;
  Rpm_to_Pwm_Map rpm_to_pwm;
  Temp_to_Rpm_Map temp_to_rpm;
  Pwm start_pwm = 0;
  milliseconds interval{0};
  bool enabled = false;

  struct {
    bool just_started{true};
    int rem_intervals{0};
    Rpm targeted_rpm{0};
    int top_stickiness_rem_intervals{0};
  } smoothing;

  virtual bool set_pwm(const Pwm pwm);
  Pwm find_closest_pwm(Rpm rpm);
  bool recover_control();
  Rpm smooth_rpm(const Rpm rpm);
  void sleep_for_interval() const;

  optional<Rpm> set_stabilised_pwm(const Pwm pwm);
  bool set_pwm_test();
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

using FanMap = std::unordered_map<string, unique_ptr<fc::FanInterface>>;

#endif // FANCON_FANINTERFACE_HPP
