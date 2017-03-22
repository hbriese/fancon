#ifndef fancon_MAIN_HPP
#define fancon_MAIN_HPP

#include <algorithm>    // transform, sort
#include <csignal>
#include <cmath>        // floor
#include <iomanip>      // setw, left
#include <string>
#include <sstream>
#include <functional>   // reference_wrapped
#include <thread>
#include <sensors/sensors.h>
#include <unistd.h>
#include <sys/stat.h>
#include "Util.hpp"
#include "Controller.hpp"

using std::string;
using std::to_string;
using std::stringstream;
using std::setw;
using std::left;
using std::next;
using std::thread;
using std::move;
using std::reference_wrapper;
using fancon::Controller;
using fancon::MappedFan;
using fancon::FanTestResult;
using fancon::DaemonState;
using fancon::Util::conf_path;

namespace Util = fancon::Util;

int main(int argc, char *argv[]);

namespace fancon {
DaemonState daemon_state;

void help();

void listFans(Controller &sensorController);
void listSensors(Controller &sensorController);

vector<ulong> getThreadTasks(uint nThreads, ulong nTasks);

template<typename T>
vector<pair<typename T::iterator, typename T::iterator>> getTasks(uint threads, T &vec) {
  vector<pair<typename T::iterator, typename T::iterator>> threadTasks;
  auto tasks = vec.size();

  // Cannot have more threads than tasks
  if (tasks < threads)
    threads = static_cast<uint>(tasks);

  // Allocate base number of tasks for each thread
  ulong baseTasks = tasks / threads;   // tasks per thread
  auto tasksRem = tasks % threads;
  vector<ulong> nThreadTasks(threads - tasksRem, baseTasks);

  // Hand off remaining tasks
  if (tasksRem)   // insert threads that have extra tasks
    nThreadTasks.insert(nThreadTasks.end(), tasksRem, baseTasks + 1);

  auto begIt = vec.begin();
  for (const auto &nTasks : nThreadTasks) {
    auto endIt = next(begIt, nTasks);
    threadTasks.emplace_back(std::make_pair(begIt, endIt));
    begIt = endIt;  // Next threads begIt is the previous thread's endIt
  }

  return threadTasks;
}

void test(Controller &sensorController, uint testRetries, bool singleThread = 0);
void testUID(UID &uid, uint retries = 4);

void handleSignal(int sig);
void start(Controller &sc, const bool fork_);
void send(DaemonState state);

struct Command {
  Command(const string &name, bool shrtName = false, const bool requireRoot = true)
      : name(name), called(false), require_root(requireRoot) {
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
  const bool require_root;
};

struct Option : Command {
public:
  Option(const string &name, bool hasValue = false, bool shrtName = true)
      : Command(name, shrtName, false), has_value(hasValue) {}

  bool has_value;
  uint val{0};
};
}

#endif //fancon_MAIN_HPP
