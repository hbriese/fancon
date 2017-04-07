#ifndef FANCON_MAIN_HPP
#define FANCON_MAIN_HPP

#include <algorithm>    // transform, sort
#include <iomanip>      // setw, left
#include <functional>   // reference_wrapped
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include "Util.hpp"
#include "Find.hpp"
#include "Controller.hpp"

#ifdef FANCON_PROFILE
#include <gperftools/profiler.h>
#include <gperftools/heap-profiler.h>
#endif //FANCON_PROFILE

using std::to_string;
using std::setw;
using std::left;
using std::reference_wrapper;
using std::function;
using boost::filesystem::create_directory;
using fancon::Find;
using fancon::Controller;
using fancon::ControllerState;
using fancon::FanTestResult;
using fancon::Util::conf_path;

namespace Util = fancon::Util;

int main(int argc, char *argv[]);

namespace fancon {
void help();

void listFans();
void listSensors();

void testFans(uint testRetries, bool singleThreaded);
void testFan(const UID &uid, unique_ptr<FanInterface> &&fan, uint retries);
void appendConfig(const string &configPath);

void start(const bool fork);
void sendSignal(ControllerState state);

struct Command {
  Command(const char *name, const char *shortName, function<void()> func,
          bool lock = false, bool requireRoot = true, bool called = false)
      : name(name), short_name(shortName), require_root(requireRoot), lock(lock), called(called), func(move(func)) {}

  const string name, short_name;
  const bool require_root, lock;
  bool called;
  function<void()> func;

  bool operator==(const string &other) { return other == short_name || other == name; }
};

struct Option {
  Option(const char *name, const char *shortName, bool hasValue = false, bool called = false)
      : name(name), short_name(shortName), called(called), has_value(hasValue) {}

  const string name, short_name;
  bool called, has_value;
  uint val{};

  bool operator==(const string &other) { return other == short_name || other == name; }
};
} // namespace fancon

#endif //FANCON_MAIN_HPP
