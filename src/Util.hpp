#ifndef fancon_UTIL_HPP
#define fancon_UTIL_HPP

#include <iostream>     // cout, endl
#include <algorithm>    // all_of
#include <mutex>
#include <thread>
#include <string>
#include <sstream>
#include <fstream>      // ifstream, ofstream
#include <vector>
#include <cstdlib>
#include <csignal>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include "Logging.hpp"

namespace bfs = boost::filesystem;

using std::string;
using std::stringstream;
using std::to_string;
using std::cout;
using std::endl;
using std::ofstream;
using std::ifstream;
using std::vector;
using boost::filesystem::create_directory;
using boost::filesystem::exists;
using boost::filesystem::path;

namespace fancon {
enum DaemonState { RUN, STOP = SIGTERM, RELOAD = SIGHUP };
enum DeviceType { FAN, FAN_NVIDIA, SENSOR, SENSOR_NVIDIA };

namespace Util {
constexpr const char *conf_path = "/etc/fancon.conf";
constexpr const char *fancon_dir = "/etc/fancon.d/";
constexpr const char *hwmon_path = "/sys/class/hwmon/hwmon";
constexpr const char *fancon_hwmon_path = "/etc/fancon.d/hwmon";
constexpr const char *temp_sensor_label = "temp";
constexpr const char *nvidia_label = "nvidia";
constexpr const char *pid_file = "/var/run/fancon.pid";

static std::mutex coutLock;

int getLastNum(const string &str);
bool isNum(const string &str);

/* locked: returns true if process that has invoked lock() is currently running  */
bool locked();
void lock();

void coutThreadsafe(const string &out);

bool validIters(const string::iterator &end, std::initializer_list<string::iterator> iterators);

string getDir(const string &hwID, DeviceType devType, const bool useSysFS = false);
string getPath(const string &path_pf, const string &hwID, DeviceType devType = FAN, const bool useSysFS = false);

string readLine(string path, int nFailed = 0);

template<typename T>
T read(const string &path, int nFailed = 0) {
  ifstream ifs(path);
  T ret{};  // initialize to default in case fails
  ifs >> ret;
  ifs.close();

  if (ifs.fail()) {
    auto exist = exists(path);
    if (exist && nFailed <= 3)  // retry 3 times
      return read<T>(path, ++nFailed);
    else {  // fail immediately if file does not exist
      const char *reason = (exist) ? "filesystem or permission error" : "doesn't exist";
      LOG(llvl::error) << "Failed to read from: " << path << " - " << reason << "; user id " << getuid();
    }
  }

  return ret;
}

template<typename T>
T read(const string &path_pf, const string &hwmon_id,
       DeviceType devType = DeviceType::FAN, bool useSysFS = false) {
  return read<T>(getPath(path_pf, hwmon_id, devType, useSysFS));
}

template<typename T>
void write(const string &path, T val, int nFailed = 0) {
  ofstream ofs(path);
  ofs << val << endl;
  ofs.close();

  if (ofs.fail()) {
    if (nFailed <= 3)   // retry 3 times
      return write<T>(path, std::move(val), ++nFailed);
    else
      LOG(llvl::error) << "Failed to write '" << val << "' to: " << path
                       << " - filesystem of permission error; user id " << getuid();
  }
}

template<typename T>
void write(const string &path_pf, const string &hwmon_id, T val,
           DeviceType devType = DeviceType::FAN, bool useSysFS = false) {
  return write<T>(getPath(path_pf, hwmon_id, devType, useSysFS), val);
}

template<typename T>
void moveAppend(vector<T> &src, vector<T> &dst) {
  if (!dst.empty()) {
    dst.reserve(dst.size() + src.size());
    std::move(std::begin(src), std::end(src), std::back_inserter(dst));
    src.clear();
  } else
    dst = std::move(src);
}
}   // UTIL
}   // fancon

#endif //fancon_UTIL_HPP
