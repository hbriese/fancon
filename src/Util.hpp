#ifndef fancon_UTIL_HPP
#define fancon_UTIL_HPP

#include <algorithm>    // all_of
#include <chrono>
#include <thread>
#include <string>
#include <sstream>
#include <fstream>      // ifstream, ofstream
#include <utility>      // pair
#include <vector>
#include <cstdlib>
#include <csignal>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include "Logging.hpp"

using std::move;
using std::ifstream;
using std::ofstream;
using std::string;
using std::stringstream;
using std::to_string;
using std::this_thread::sleep_for;
using std::chrono::seconds;
using std::unique_ptr;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;
using std::pair;
using std::vector;
using boost::filesystem::create_directory;
using boost::filesystem::exists;

namespace fancon {
enum DaemonState { RUN, STOP = SIGTERM, RELOAD = SIGHUP };
enum DeviceType {
  FAN = (1u << 0), FAN_NV = (1u << 1), FAN_INTERFACE = FAN | FAN_NV,
  SENSOR = (1u << 2), SENSOR_NV = (1u << 3), SENSOR_INTERFACE = SENSOR | SENSOR_NV
};

namespace Util {
constexpr const char *conf_path = "/etc/fancon.conf";
constexpr const char *fancon_dir = "/etc/fancon.d/";
constexpr const char *hwmon_path = "/sys/class/hwmon/hwmon";
constexpr const char *fancon_hwmon_path = "/etc/fancon.d/hwmon";
constexpr const char *temp_sensor_label = "temp";
constexpr const char *nvidia_label = "nvidia";
constexpr const char *pid_file = "/var/run/fancon.pid";

int getLastNum(const string &str);
bool isNum(const string &str);

/* locked: returns true if process that has invoked lock() is currently running  */
bool locked();
void lock();

template<typename IT>
bool notEqualTo(std::initializer_list<const IT> values, const IT to);

string getDir(const string &hwID, DeviceType devType, const bool useSysFS = false);
string getPath(const string &path_pf, const string &hwID,
               DeviceType devType = DeviceType::FAN, const bool useSysFS = false);

string readLine(string path, int nFailed = 0);  // TODO: string -> string&

template<typename T>
T read(const string &path, int nFailed = 0);

template<typename T>
T read(const string &path_pf, const string &hw_id,
       DeviceType devType = DeviceType::FAN, bool useSysFS = false) {
  return read<T>(getPath(path_pf,
                         hw_id,
                         devType,
                         useSysFS));
}

template<typename T>
bool write(const string &path, T val, int nFailed = 0);

template<typename T>
bool write(const string &path_pf, const string &hwmon_id, T val,
           DeviceType devType = DeviceType::FAN, bool useSysFS = false) {
  return write<T>(getPath(path_pf,
                          hwmon_id,
                          devType,
                          useSysFS), val);
}

template<typename T>
void moveAppend(vector<T> &src, vector<T> &dst);
}
}

//----------------------
// TEMPLATE DEFINITIONS
//----------------------

template<typename IT>
bool fancon::Util::notEqualTo(std::initializer_list<const IT> values, const IT to) {
  for (auto it : values)
    if (it == to)
      return false;

  return true;
}

template<typename T>
T fancon::Util::read(const string &path, int nFailed) {
  ifstream ifs(path);
  T ret{};
  ifs >> ret;
  ifs.close();

  if (ifs.fail()) {
    auto exist = exists(path);
    // Retry 3 times if file exists
    if (exist && nFailed <= 3)
      return read<T>(path, ++nFailed);

    const char *reason = (exist) ? "filesystem or permission error" : "doesn't exist";
    LOG(llvl::error) << "Failed to read from: " << path << " - " << reason << "; user id " << getuid();
  }

  return ret;
}

template<typename T>
bool fancon::Util::write(const string &path, T val, int nFailed) {
  ofstream ofs(path);
  ofs << val;
  ofs.close();

  if (ofs.fail()) {
    if (nFailed <= 3)   // retry 3 times
      return write<T>(path, move(val), ++nFailed);

    LOG(llvl::debug) << "Failed to write '" << val << "' to: " << path
                     << " - filesystem of permission error; user id " << getuid();
    return false;
  }

  return true;
}

template<typename T>
void fancon::Util::moveAppend(vector<T> &src, vector<T> &dst) {
  if (!dst.empty()) {
    dst.reserve(dst.size() + src.size());
    move(std::begin(src), std::end(src), std::back_inserter(dst));
    src.clear();
  } else
    dst = move(src);
}

#endif //fancon_UTIL_HPP
