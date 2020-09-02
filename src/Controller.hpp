#ifndef FANCON_CONTROLLER_HPP
#define FANCON_CONTROLLER_HPP

#include "Devices.hpp"
#include "FanTask.hpp"
#include "Util.hpp"
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
using fc::FanTask;
using std::find_if;
using std::future;
using std::istringstream;
using std::list;
using FanStatus = fc_pb::FanStatus_Status;
using DevicesCallback = function<void(const fc::Devices &)>;
using StatusCallback = function<void(const FanInterface &, const FanStatus)>;

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
  map<string, FanTask> tasks;
  list<DevicesCallback> device_observers;
  list<StatusCallback> status_observers;
  Util::RemovableMutex device_observers_mutex, status_observers_mutex;

  FanStatus status(const string &flabel) const;
  void enable(fc::FanInterface &fan, bool enable_all_dell = true);
  void enable_all();
  void disable(const string &flabel, bool disable_all_dell = true);
  void disable_all();
  void reload();
  void recover();
  void nv_init();
  void test(fc::FanInterface &fan, bool forced, bool blocking,
            shared_ptr<Util::ObservableNumber<int>> test_status);
  size_t tests_running() const;
  void set_devices(const fc_pb::Devices &devices_);

  void from(const fc_pb::ControllerConfig &c);
  void to(fc_pb::Controller &c) const;
  void to(fc_pb::ControllerConfig &c) const;

private:
  path config_path;
  optional<thread> watcher;
  fs::file_time_type config_write_time;

  void enable_dell_fans();
  void disable_dell_fans();
  bool is_testing(const string &flabel) const;
  optional<fc_pb::Controller> read_config();
  void merge(Devices &old_it, bool replace_on_match, bool deep_cmp = false);
  void remove_devices_not_in(
      std::initializer_list<std::reference_wrapper<Devices>> list_of_devices);
  void to_file(bool backup);
  void update_config_write_time();
  bool config_file_modified() const;
  thread spawn_watcher();
  void notify_devices_observers();
  void notify_status_observers(const string &flabel);
  static string date_time_now();
};
} // namespace fc

#endif // FANCON_CONTROLLER_HPP
