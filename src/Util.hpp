#ifndef fancon_UTIL_HPP
#define fancon_UTIL_HPP

#include <iostream>     // endl
#include <algorithm>    // all_of
#include <mutex>
#include <thread>
#include <string>
#include <sstream>
#include <fstream>      // ifstream, ofstream
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <csignal>

#define LOG(lvl)  BOOST_LOG_TRIVIAL(lvl)

using std::clog;
using std::string;
using std::to_string;
using std::cout;
using std::endl;
using std::ofstream;
using std::ifstream;
using std::vector;
using boost::filesystem::create_directory;
using boost::filesystem::exists;
using boost::filesystem::path;
using boost::log::trivial::severity_level;

namespace fancon {
enum DaemonState { RUN, STOP = SIGINT, RELOAD = SIGHUP };
enum DeviceType { FAN, FAN_NVIDIA, TEMP_SENSOR };

namespace Util {
constexpr const char *hwmon_path = "/sys/class/hwmon/hwmon";
constexpr const char *nvidia_label = "nvidia";
constexpr const char *fancon_dir = "/etc/fancon.d/";
constexpr const char *fancon_path = "/etc/fancon.d/hwmon";

static std::mutex coutLock;

static const int speed_change_t = 3;    // seconds to allow for rpm changes when altering the pwm
static const int pwm_max_absolute = 255;

int getLastNum(const string &str);
bool isNum(const string &str);
void coutThreadsafe(const string &out);

/* found: returns false if any of the iterators are invalid */
bool validIter(const string::iterator &end, std::initializer_list<string::iterator> iterators);

string getDir(const string &hwID, DeviceType devType, const bool useSysFS = false);
string getPath(const string &path_pf, const string &hwID, DeviceType devType = FAN, const bool useSysFS = false);

string readLine(string path);

template<typename T>
T read(const string &path, int nFailed = 0) {
  ifstream ifs(path);
  T ret{};  // initialize to default in case fails
  ifs >> ret;
  ifs.close();

  if (ifs.fail()) {
    if (nFailed > 4)
      LOG(severity_level::debug) << "Failed to read from: " << path
                                 << ((exists(path)) ? " - filesystem or permission error" : " - doesn't tested!");
    else
      return read<T>(path, ++nFailed);
  }

  return ret;
}

template<typename T>
inline T read(const string &path_pf, const string &hwmon_id,
              DeviceType devType = DeviceType::FAN, bool useSysFS = false) {
  return read<T>(getPath(path_pf, hwmon_id, devType, useSysFS));
}

template<typename T>
void write(const string &path, T val, int nFailed = 0) {
  ofstream ofs(path);
  ofs << val << endl;
  ofs.close();

  if (ofs.fail()) {
    if (nFailed > 4)
      LOG(severity_level::debug) << "Failed to write '" << val << "' to: " << path
                                 << " - filesystem of permission error";
    else
      return write<T>(path, std::move(val), ++nFailed);
  }
}

template<typename T>
inline void write(const string &path_pf, const string &hwmon_id, T val,
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
