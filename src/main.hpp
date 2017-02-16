#ifndef FANCON_MAIN_HPP
#define FANCON_MAIN_HPP

#include <algorithm>    // transform, sort
#include <csignal>
#include <cmath>        // floor
#include <iostream>
#include <string>
#include <sstream>
#include <functional>   // ref, reference_wrapped
#include <future>       // future, promise
#include <thread>
#include <boost/asio.hpp>
#include <sensors/sensors.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libexplain/setsid.h>
#include "Util.hpp"
#include "SensorController.hpp"

namespace basio = boost::asio;

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::to_string;
using std::stringstream;
using std::next;
using std::thread;
using std::future;
using std::promise;
using std::make_pair;
using std::move;
using std::ref;
using std::reference_wrapper;
using boost::asio::local::stream_protocol;
using fancon::SensorController;
using fancon::TemperatureSensor;
using fancon::Util::DaemonState;
using fancon::Util::log;

int main(int argc, char *argv[]);

namespace fancon {
static const string conf_path("/etc/fancon.conf");

static const string pid_file = string(fancon::Util::fancon_dir) + "pid";

static DaemonState daemon_state;

string help();

void FetchTemp();

string listFans(SensorController &sensorController);
string listSensors(SensorController &sensorController);

bool pidExists(pid_t pid);
void writeLock();

void test(SensorController &sensorController, const bool profileFans = false);
void testUID(UID &uid, const bool profileFans = false);

void handleSignal(int sig);
void start(const bool debug = false, const bool fork);
void send(DaemonState state);

struct Param {
  Param(const string &name, bool called = false)
      : name(name), called(called) {}

  bool operator==(const string &other) { return other == name; }

  const string name;
  bool called;
};

class Arg {
public:
  Arg(string name, const bool shrtName = false, bool called = false)
      : name(name), called(called) {
    if (shrtName) {
      shrt_name += name.front();

      // if name contains '-' set first 2 chars of each word as shrt_name
      auto it = name.begin();
      while ((it = find(next(it), name.end(), '-')) != name.end() && next(it) != name.end())
        shrt_name += *(next(it));
    }
  }

  bool operator==(const string &other) { return other == shrt_name || other == name; }

  bool called;

private:
  const string name;
  string shrt_name;
};
}

#endif //FANCON_MAIN_HPP
