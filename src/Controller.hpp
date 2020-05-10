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

#include "Devices.hpp"
#include "proto/DevicesSpec.pb.h"

using boost::thread;
using fc::FanInterface;
using std::find_if;
using std::future;
using std::istringstream;

namespace fc {
extern bool dynamic;
extern uint smoothing_intervals;
extern uint top_stickiness_intervals;
extern uint temp_averaging_intervals;

enum class ControllerState : sig_atomic_t {
  RUN,
  STOP,
  RELOAD
} extern controller_state;

class Controller {
public:
  Controller(path conf_path_);

  path config_path;
  milliseconds update_interval{1000};
  Devices devices;

  void start();
  void test(bool force = false, bool safely = false);

  static bool running();
  static void stop();
  static void reload();
  static bool reloading();
  static void reload_nvidia();

private:
  fs::file_time_type config_write_time;

  void from(const fc_pb::Controller &c);
  void from(const fc_pb::ControllerConfig &c);
  void to(fc_pb::Controller &c) const;
  void to(fc_pb::ControllerConfig &c) const;

  void load_devices();
  void to_file(path file, bool backup = true);

  void update_config_write_time();
  bool config_file_modified() const;
  static void shutdown_threads(vector<thread> &threads);
  string date_time_now() const;

  static bool tests_running(const vector<shared_ptr<double>> &test_completion);
  static double sum(const vector<shared_ptr<double>> &numbers);
};
} // namespace fc

#endif // FANCON_CONTROLLER_HPP
