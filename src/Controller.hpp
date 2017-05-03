#ifndef FANCON_CONTROLLER_HPP
#define FANCON_CONTROLLER_HPP

#include "Devices.hpp"
#include <algorithm> // find_if
#include <chrono>
#include <csignal>
#include <sstream> // istringstream
#include <thread>

using std::istringstream;
using std::find_if;
using std::thread;
using std::this_thread::sleep_for;
using std::this_thread::sleep_until;

namespace fancon {
struct MappedFan;

using sensor_container_t = vector<unique_ptr<SensorInterface>>;
using fan_container_t = vector<MappedFan>;
using time_point_t = chrono::time_point<chrono::steady_clock, milliseconds>;

enum class ControllerState {
  run,
  stop = SIGTERM,
  reload = SIGHUP
} extern controller_state;

class Controller {
public:
  explicit Controller(const string &configPath);

  controller::Config conf;

  time_point_t sensors_wakeup, fans_wakeup;

  sensor_container_t sensors;
  fan_container_t fans;

  ControllerState run();
  void reload(const string &configPath);

  void updateSensors(vector<sensor_container_t::iterator> sensors);
  void updateFans(vector<fan_container_t::iterator> fans);

  void scheduler();

  static void signalHandler(int sig);
  static bool validConfigLine(const string &line);

private:
  void deferStart(time_point_t &timePoint);
};

struct MappedFan {
  MappedFan(unique_ptr<FanInterface> fan, const SensorInterface &sensor)
      : fan(move(fan)), sensor(sensor) {}

  unique_ptr<FanInterface> fan;
  const SensorInterface &sensor;
};
}

#endif // FANCON_CONTROLLER_HPP
