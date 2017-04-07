#ifndef FANCON_CONTROLLER_HPP
#define FANCON_CONTROLLER_HPP

#include <algorithm>    // find_if
#include <csignal>
#include <sstream>      // istringstream
#include <chrono>
#include <thread>
#include "Find.hpp"

using std::istringstream;
using std::find_if;
using std::thread;
using std::this_thread::sleep_for;
using std::this_thread::sleep_until;

namespace fancon {
struct MappedFan;

using sensor_container_t = vector<unique_ptr<SensorInterface>>;
using fan_container_t = vector<MappedFan>;

enum class ControllerState {
  run, stop = SIGTERM, reload = SIGHUP, defered_start
} extern controller_state;

class Controller {
public:
  Controller(const string &configPath);

  controller::Config conf;
  vector<thread> threads;

  chrono::time_point <chrono::steady_clock, milliseconds> main_wakeup, sensors_wakeup, fans_wakeup;

  sensor_container_t sensors;
  fan_container_t fans;

  ControllerState run();
  void reload(const string &configPath);

  static void signalHandler(int sig);

  static bool validConfigLine(const string &line);

private:
  void readSensors(vector<sensor_container_t::iterator> &sensors);
  void updateFans(vector<fan_container_t::iterator> &fans);

  void startThreads();
  void updateWakeupTimes();

  void deferStart(chrono::time_point <chrono::steady_clock, milliseconds> &timePoint);
};

struct MappedFan {
  MappedFan(unique_ptr<FanInterface> fan, const SensorInterface &sensor)
      : fan(move(fan)), sensor(sensor) {}

  unique_ptr<FanInterface> fan;
  const SensorInterface &sensor;
};
}

#endif //FANCON_CONTROLLER_HPP
