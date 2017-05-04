#ifndef FANCON_MAIN_HPP
#define FANCON_MAIN_HPP

#include "Controller.hpp"
#include "Devices.hpp"
#include "Util.hpp"
#include <algorithm> // transform, sort
#include <boost/filesystem.hpp>
#include <functional> // reference_wrapped
#include <iomanip>    // setw, left
#include <sys/stat.h>

#ifdef FANCON_PROFILE
#include <gperftools/heap-profiler.h>
#include <gperftools/profiler.h>
#endif // FANCON_PROFILE

using std::to_string;
using std::setw;
using std::left;
using std::reference_wrapper;
using std::function;
using boost::filesystem::create_directory; // TODO C++17: std::create_directory
using fancon::Controller;
using fancon::ControllerState;
using fancon::Devices;
using fancon::TestResult;
using fancon::Util::config_path;

int main(int argc, char *argv[]);

namespace fancon {
/// \var 664 (rw-rw-r--) files; 775 (rwxrwxr-x) directories
constexpr const __mode_t default_umask{002};

void help();
void suggestUsage(const char *fanconDir, const char *configPath);
constexpr size_t strlength(const char *s) // TODO C++17 - remove
{ return (*s == 0) ? 0 : strlength(s + 1) + 1; }

void listFans();
void listSensors();

void appendConfig(const string &configPath);

void testFans(uint testRetries, bool singleThreaded);
void testFan(const UID &uid, unique_ptr<FanInterface> &&fan, uint retries);

void forkOffParent();
void start(const bool fork);
void signalState(ControllerState state);

struct Command {
  Command(const char *name, const char *shortName, function<void()> func,
          bool lock = false, bool requireRoot = true, bool called = false)
      : name(name), short_name(shortName), require_root(requireRoot),
        lock(lock), called(called), func(move(func)) {}

  const string name, short_name;
  const bool require_root, lock;
  bool called;
  function<void()> func;

  bool operator==(const string &other) {
    return other == short_name || other == name;
  }
};

struct Option {
  Option(const char *name, const char *shortName, bool hasValue = false,
         bool called = false)
      : name(name), short_name(shortName), called(called), has_value(hasValue) {
  }

  const string name, short_name;
  bool called, has_value;
  uint val{};

  bool operator==(const string &other) {
    return other == short_name || other == name;
  }

  bool setIfValid(const string &str);
};
} // namespace fancon

#endif // FANCON_MAIN_HPP
