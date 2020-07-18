#ifndef FANCON_CONTROLLER_HPP
#define FANCON_CONTROLLER_HPP

#include <algorithm>
#include <boost/thread.hpp>
#include <cmath>
#include <future>
#include <google/protobuf/text_format.h>
#include <map>
#include <sstream>
#include <thread>
#include <utility>

#include "Devices.hpp"
#include "proto/DevicesSpec.pb.h"

using boost::thread;
using fc::FanInterface;
using fc::Util::Observable;
using std::find_if;
using std::future;
using std::istringstream;

namespace fc {
extern bool dynamic;
extern uint smoothing_intervals;
extern uint top_stickiness_intervals;
extern uint temp_averaging_intervals;

enum class ControllerState {
  ENABLED = fc_pb::ControllerState_State_ENABLED,
  DISABLED = fc_pb::ControllerState_State_DISABLED,
  RELOAD = fc_pb::ControllerState_State_RELOAD
};

struct FanThread {
  explicit FanThread(thread &&t,
                     shared_ptr<Observable<int>> testing_status = nullptr)
      : t(move(t)), test_status(move(testing_status)) {}
  ~FanThread() { t.interrupt(); } // Interrupt thread when unwound

  thread t;
  shared_ptr<Observable<int>> test_status;

  bool is_testing() const { return bool(test_status); }
  void join() { t.join(); }

  FanThread &operator=(FanThread &&other) noexcept;
};

class Controller {
public:
  explicit Controller(path conf_path_);
  ~Controller();

  ControllerState state;
  Devices devices;

  void control(fc::FanInterface &fan);
  void test(fc::FanInterface &fan, bool forced, function<void(int &)> cb);
  bool testing(const string &label) const;
  size_t tests_running() const;

  void enable();
  bool enabled() const;
  void disable();
  bool disabled() const;
  void reload();
  void reload_added();
  bool reloading() const;
  void nv_init();

  void from(const fc_pb::Controller &c);
  void from(const fc_pb::ControllerConfig &c);
  void to(fc_pb::Controller &c) const;
  void to(fc_pb::ControllerConfig &c) const;

private:
  path config_path;
  milliseconds update_interval{1000};
  std::map<string, FanThread> threads;
  fs::file_time_type config_write_time;

  void load_conf_and_enumerated();
  void to_file(bool backup = true);

  void update_config_write_time();
  bool config_file_modified() const;
  void join_threads();
  static string date_time_now();
};
} // namespace fc

#endif // FANCON_CONTROLLER_HPP
