#ifndef FANCON_CONTROLLER_HPP
#define FANCON_CONTROLLER_HPP

#include "Devices.hpp"
#include "FanThread.hpp"
#include "proto/DevicesSpec.pb.h"
#include <algorithm>
#include <boost/thread.hpp>
#include <cmath>
#include <future>
#include <google/protobuf/text_format.h>
#include <list>
#include <map>
#include <sstream>
#include <thread>
#include <utility>

using fc::FanInterface;
using fc::FanThread;
using std::find_if;
using std::future;
using std::istringstream;
using std::list;
using FanStatus = fc_pb::FanStatus_Status;
using FThreads_map = std::map<string, FanThread>;

namespace fc {
extern milliseconds update_interval;
extern bool dynamic;
extern uint smoothing_intervals;
extern uint top_stickiness_intervals;
extern uint temp_averaging_intervals;

class Controller {
public:
  explicit Controller(path conf_path_);
  ~Controller();

  Devices devices;
  FThreads_map fthreads;
  list<function<void(const fc::Devices &)>> devices_observers;
  list<function<void(const FanInterface &, const FanStatus)>> status_observers;

  FanStatus status(const string &flabel) const;
  void enable(fc::FanInterface &fan, bool enable_all_dell = true);
  void enable_all();
  void disable(const string &flabel, bool disable_all_dell = true);
  void disable_all();
  void reload();
  void reload_added();
  void nv_init();
  void test(fc::FanInterface &fan, bool forced, function<void(int &)> cb);
  size_t tests_running() const;
  void set_devices(const fc_pb::Devices &devices_);

  void from(const fc_pb::Controller &c);
  void from(const fc_pb::ControllerConfig &c);
  void to(fc_pb::Controller &c) const;
  void to(fc_pb::ControllerConfig &c) const;

private:
  path config_path;
  optional<thread> watcher;
  fs::file_time_type config_write_time;

  void enable_dell_fans();
  void disable_dell_fans();
  void load_conf_and_enumerated();
  void to_file(bool backup);
  void update_config_write_time();
  bool config_file_modified() const;
  thread spawn_watcher();
  void notify_devices_observers() const;
  void notify_status_observers(const string &flabel);
  static string date_time_now();
};
} // namespace fc

#endif // FANCON_CONTROLLER_HPP
