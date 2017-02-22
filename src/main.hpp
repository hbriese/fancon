#ifndef fancon_MAIN_HPP
#define fancon_MAIN_HPP

#include <algorithm>    // transform, sort
#include <csignal>
#include <cmath>        // floor
#include <iostream>
#include <string>
#include <sstream>
#include <functional>   // ref, reference_wrapped
#include <future>       // future, promise
#include <thread>
#include <sensors/sensors.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include "Util.hpp"
#include "SensorController.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::future;
using std::string;
using std::to_string;
using std::stringstream;
using std::next;
using std::thread;
using std::make_pair;
using std::move;
using std::ref;
using std::reference_wrapper;
using fancon::SensorController;
using fancon::TemperatureSensor;
using fancon::TestResult;
using fancon::Util::DaemonState;
using fancon::Util::log;

int main(int argc, char *argv[]);

namespace fancon {
const char *conf_path = "/etc/fancon.conf";

const string pid_file = string(fancon::Util::fancon_dir) + "pid";

DaemonState daemon_state;

string help();
void firstTimeSetup();

string listFans(SensorController &sensorController);
string listSensors(SensorController &sensorController);

bool pidExists(pid_t pid);
void writeLock();
vector<ulong> getThreadTasks(uint nThreads, ulong nTasks);

void test(SensorController &sensorController, const bool profileFans, uint testRetries, bool singleThread = 0);
void testUID(UID &uid, const bool profileFans = false, uint retries = 4);

void handleSignal(int sig);
void start(SensorController &sc, const bool fork_ = false, uint nThreads = 0, const bool writeLock = true);
void send(DaemonState state);

struct Command {
  Command(const string &name, bool shrtName = false)
      : name(name), called(false) {
    if (shrtName) {
      shrt_name += name.front();

      // if name contains '-' set first 2 chars of each word as shrt_name
      auto it = name.begin();
      while ((it = find(next(it), name.end(), '-')) != name.end() && next(it) != name.end())
        shrt_name += *(next(it));
    }
  }

  bool operator==(const string &other) { return other == shrt_name || other == name; }

  const string name;
  string shrt_name;
  bool called;
};

struct Option : Command {
public:
  Option(const string &name, bool shrtName = false, bool hasValue = false)
      : Command(name, shrtName), has_value(hasValue) {}

  bool has_value;
  uint val = 0;
};
}

#endif //fancon_MAIN_HPP
