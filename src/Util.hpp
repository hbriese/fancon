#ifndef fancon_UTIL_HPP
#define fancon_UTIL_HPP

#include <iostream>     // endl
#include <algorithm>    // all_of, reverse
#include <exception>    // runtime_exception
#include <mutex>
#include <string>
#include <sstream>
#include <fstream>      // ifstream, ofstream
//#include <memory>       // shared_ptr
#include <boost/filesystem.hpp>
#include <iostream>
#include <sys/syslog.h>
#include <csignal>

namespace bfs = boost::filesystem;

using std::string;
using std::to_string;
using std::cout;
using std::endl;
using std::ofstream;
using std::ifstream;
//using std::shared_ptr;
using boost::filesystem::exists;
using boost::filesystem::path;

namespace fancon {
namespace Util {
enum DaemonState { RUN, STOP = SIGINT, RELOAD = SIGHUP };

constexpr const char *hwmon_path = "/sys/class/hwmon/hwmon";

constexpr const char *fancon_dir = "/etc/fancon.d/";

constexpr const char *fancon_path = "/etc/fancon.d/hwmon";

static std::mutex coutLock;

int getLastNum(string str);   // TODO: take as reference and do not reverse
bool isNum(const string &str);
void coutThreadsafe(const string &out);

/* found: returns false if any of the iterators are invalid */
bool validIter(const string::iterator &end, std::initializer_list<string::iterator> iterators);

void openSyslog(bool debug = false);
void closeSyslog();
inline void log(int logSeverity, const string &message) {
  syslog(logSeverity, "%s", message.c_str());
}

/* getPath: returns the usual path (used by the driver) if it exists, else a /etc/fancon.d/ path */
inline string getPath(const string &path_pf, const string &hwmon_id, const bool useSysFS = false) {
  return string(((useSysFS) ? hwmon_path + hwmon_id + '/' + path_pf
                            : fancon_path + hwmon_id + '/' + path_pf));
}

string readLine(string path);

template<typename T>
T read(const string &path, int nFailed = 0) {
  ifstream ifs(path);
  T ret;
  ifs >> ret;
  ifs.close();

  if (ifs.fail()) {
    if (nFailed > 4)
      fancon::Util::log(LOG_ERR, string("Failed to read from: ") + path
          + ((exists(path)) ? " - filesystem or permission error" : " - doesn't exist!"));
    else
      return read<T>(path, ++nFailed);
  }

  return ret;
}

template<typename T>
inline T read(const string &path_pf, const string &hwmon_id, bool useSysFS = false) {
  return read<T>(getPath(path_pf, hwmon_id, useSysFS));
}

template<typename T>
void write(const string &path, T val, int nFailed = 0) {
  ofstream ofs(path);
  ofs << val << endl;
  ofs.close();

  if (ofs.fail()) {
    if (nFailed > 4)
      fancon::Util::log(LOG_ERR, string("Failed to write '") + to_string(val)
          + "' to: " + path + " - filesystem or permission error");
    else
      return write<T>(path, std::move(val), ++nFailed);
  }
}

template<typename T>
inline void write(const string &path_pf, const string &hwmon_id, T val, bool useSysFS = false) {
  return write<T>(getPath(path_pf, hwmon_id, useSysFS), val);
}
}   // UTIL
}   // fancon

#endif //fancon_UTIL_HPP
