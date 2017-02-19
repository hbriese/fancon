#ifndef FANCTL_UTIL_HPP
#define FANCTL_UTIL_HPP

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

namespace fanctl {
namespace Util {
enum DaemonState { RUN, STOP = SIGINT, RELOAD = SIGHUP };

constexpr const char *hwmon_path = "/sys/class/hwmon/hwmon";

constexpr const char *fanctl_dir = "/etc/fanctl.d/";

constexpr const char *fanctl_path = "/etc/fanctl.d/hwmon";

static std::mutex coutLock;

static bool syslog_open = false;

int getLastNum(string str);   // TODO: take as reference and do not reverse
bool isNum(const string &str);
void coutThreadsafe(const string &out);

/* found: returns false if any of the iterators are invalid */
const bool validIter(const string::iterator &end, std::initializer_list<string::iterator> iterators);

void writeSyslogConf();
void openSyslog(bool debug = false);
void closeSyslog();
void log(int logSeverity, const string &message);

/* getPath: returns the usual path (used by the driver) if it exists, else a /etc/fanctl.d/ path */
string getPath(const string &path_pf, const string &hwmon_id, const bool prev_failed = false);

string readLine(string path);

template<typename T>
T read(const string &path) {
  ifstream ifs(path);
  T ret;
  ifs >> ret;
  ifs.close();

  if (ifs.fail())
    fanctl::Util::log(LOG_ERR, string("Failed to read from: ") + path);

  return ret;
}

template<typename T>
T read(const string &path_pf, const string &hwmon_id, int nFailed = 0) {
//    return read<T>(getPath(path_pf, hwmon_id));
  string path(getPath(path_pf, hwmon_id, (nFailed > 1)));   // try initial path 2 times
  if (!exists(path))
    log(LOG_DEBUG, string("Failed to read, file does not exist: ") + path);

  ifstream ifs(path);
  T ret;
  ifs >> ret;
  ifs.close();

  if (ifs.fail() && nFailed < 5) {    // try read 4 times
    fanctl::Util::log(LOG_ERR, string("Failed to read from: ") + path);
    return read<T>(path_pf, hwmon_id, ++nFailed);
  }

  return ret;
}

template<typename T>
void write(const string &path, T val) {
  ofstream ofs(path);
  ofs << val << endl;
  ofs.close();

  if (ofs.fail())
    fanctl::Util::log(LOG_ERR, string("Failed to write '") + to_string(val) + "' to: " + path);
}

template<typename T>
void write(const string &path_pf, const string &hwmon_id, T val, const bool prev_failed = false) {
  string path(getPath(path_pf, hwmon_id, prev_failed));
  ofstream ofs(path);
  ofs << val << endl;
  ofs.close();

  if (ofs.fail()) {
    fanctl::Util::log(LOG_ERR, string("Failed to write to: ") + path);
    return write<T>(path_pf, hwmon_id, val, true);
  }
}
}   // UTIL
}   // fanctl

#endif //FANCTL_UTIL_HPP
