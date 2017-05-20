#ifndef FANCON_UTIL_HPP
#define FANCON_UTIL_HPP

#include "Logging.hpp"
#include <algorithm>            // all_of
#include <boost/filesystem.hpp> // TODO C++17: <filesystem>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>  // ifstream, ofstream
#include <iterator> // next, prev
#include <string>
#include <sys/types.h>
#include <tuple>
#include <utility> // pair
#include <vector>

using std::move;
using std::ifstream;
using std::ofstream;
using std::string;
using std::stringstream;
using std::to_string;
using std::unique_ptr;
using std::make_unique;
using std::pair;
using std::tuple;
using std::vector;
using std::next;
using std::prev;
using boost::filesystem::exists; // TODO C++17: std::exists

namespace fancon {
enum class DeviceType : int {
  fan = (1u << 0),
  fan_nv = (1u << 1),
  fan_interface = fan | fan_nv,
  sensor = (1u << 2),
  sensor_nv = (1u << 3),
  sensor_interface = sensor | sensor_nv
};

fancon::DeviceType operator|(fancon::DeviceType lhs, fancon::DeviceType rhs);
fancon::DeviceType operator&(fancon::DeviceType lhs, fancon::DeviceType rhs);

namespace Util {
constexpr const char *pid_path = "/var/run/fancon.pid";
constexpr const char *config_path = "/etc/fancon.conf";
constexpr const char *fancon_dir = "/etc/fancon.d/";
constexpr const char *hwmon_path = "/sys/class/hwmon/hwmon";
constexpr const char *fancon_hwmon_path = "/etc/fancon.d/hwmon";
constexpr const char *temp_sensor_label = "temp";
constexpr const char *nvidia_label = "nvidia";

int lastNum(const string &str);
bool isNum(const string &str);

bool locked();
void lock();
bool try_lock();

template<typename IT>
bool equalTo(std::initializer_list<const IT> values, const IT value);

string getDir(const string &hwID, DeviceType devType, const bool sysFS = false);
string getPath(const string &path_pf, const string &hwID,
               DeviceType devType = DeviceType::fan, const bool sysFS = false);

string readLine(string path, int nFailed = 0); // TODO string -> string&

template<typename T> T read(const string &path, int nFailed = 0);

template<typename T>
T read(const string &path_pf, const string &hw_id,
       DeviceType devType = DeviceType::fan, bool sysFS = false) {
  return read<T>(getPath(path_pf, hw_id, devType, sysFS));
}

template<typename T> bool write(const string &path, T val, int nFailed = 0);

template<typename T>
bool write(const string &path_pf, const string &hwmon_id, T val,
           DeviceType devType = DeviceType::fan, bool sysFS = false) {
  return write(getPath(path_pf, hwmon_id, devType, sysFS), val);
}

template<typename T> void moveAppend(vector<T> &source, vector<T> &destination);

template<typename T>
vector<vector<typename T::iterator>> distributeTasks(uint threads,
                                                     T &tasksContainer);
}
}

//----------------------//
// TEMPLATE DEFINITIONS //
//----------------------//

template<typename IT>
bool fancon::Util::equalTo(std::initializer_list<const IT> values,
                           const IT value) {
  for (const auto &it : values)
    if (it == value)
      return true;

  return false;
}

template<typename T> T
fancon::Util::read(const string &path, int nFailed) {
  ifstream ifs(path);
  T ret;
  ifs >> ret;

  // TODO C++17: test performance of std::from_chars vs >>
  //  auto len = ifs.seekg(ifs.end).tellg();
  //  vector<char> buf(len);
  //  ifs.seekg(ifs.beg).read(buf.data(), len);
  //  auto res = std::from_chars(buf.data(), buf.data() + len, ret);

  ifs.close();

  if (!ifs) {
    auto exist = exists(path);
    // Retry 3 times if file exists
    if (exist && nFailed <= 3)
      return read<T>(path, ++nFailed);

    const char *reason =
        (exist) ? "filesystem or permission error" : "doesn't exist";
    LOG(llvl::debug) << "Failed to read from: " << path << " - " << reason
                     << "; user id " << getuid();
    return T{};
  }

  return ret;
}

template<typename T>
bool fancon::Util::write(const string &path, T val, int nFailed) {
  ofstream ofs(path);
  ofs << val;
  ofs.close();

  if (!ofs) {
    if (nFailed <= 3) // Retry 3 times
      return write(path, move(val), ++nFailed);

    LOG(llvl::debug) << "Failed to write '" << val << "' to: " << path << " - "
          "filesystem of permission error; user id " << getuid();
    return false;
  }

  return true;
}

/// \brief Move source to the end of destination
template<typename T>
void fancon::Util::moveAppend(vector<T> &src, vector<T> &dst) {
  if (!dst.empty()) {
    dst.reserve(dst.size() + src.size());
    move(std::begin(src), std::end(src), std::back_inserter(dst));
    src.clear();
  } else
    dst = move(src);
}

/// \tparam T task container type
/// \param threads Max number of threads to use
/// \param tasksContainer Container holding tasks to be distributed among
/// threads
/// \return A list of iterators distributed to threads
template<typename T> // TODO check iterator support
vector<vector<typename T::iterator>>
fancon::Util::distributeTasks(uint threads, T &tasksContainer) {
  // Cannot have more threads than tasks
  auto tasks = tasksContainer.size();
  if (tasks < threads)
    threads = static_cast<uint>(tasks);

  vector<vector<typename T::iterator>> threadTasks;
  threadTasks.reserve(tasks);

  // Distribute base number of tasks for each thread
  ulong baseTasks = tasks / threads; // Tasks per thread
  auto tasksRem = tasks % threads;
  vector<ulong> nThreadTasks(threads - tasksRem, baseTasks);

  // Hand off remaining tasks
  if (tasksRem) // Insert threads that have extra tasks
    nThreadTasks.insert(nThreadTasks.end(), tasksRem, baseTasks + 1);

  auto it = tasksContainer.begin();
  for (const auto &nTasks : nThreadTasks) {
    vector<typename T::iterator> tasksVec;
    tasksVec.reserve(nTasks);

    auto end = next(it, nTasks);
    while (it != end)
      tasksVec.push_back(it++);

    threadTasks.emplace_back(move(tasksVec));

    it = end;
  }

  return threadTasks;
}

#endif // FANCON_UTIL_HPP
