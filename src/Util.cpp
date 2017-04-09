#include <boost/filesystem/operations.hpp>
#include "Util.hpp"

using namespace fancon;

fancon::DeviceType fancon::operator|(fancon::DeviceType lhs, fancon::DeviceType rhs) {
  return static_cast<fancon::DeviceType> (
      static_cast<std::underlying_type<fancon::DeviceType>::type>(lhs) |
          static_cast<std::underlying_type<fancon::DeviceType>::type>(rhs)
  );
}

fancon::DeviceType fancon::operator&(fancon::DeviceType lhs, fancon::DeviceType rhs) {
  return static_cast<fancon::DeviceType> (
      static_cast<std::underlying_type<fancon::DeviceType>::type>(lhs) &
          static_cast<std::underlying_type<fancon::DeviceType>::type>(rhs)
  );
}

int Util::lastNum(const string &str) {
  auto endDigRevIt = std::find_if(str.rbegin(), str.rend(), [](const char &c) { return std::isdigit(c); });
  auto endIt = (endDigRevIt + 1).base();

  bool numFound = false;
  auto begDigRevIt = std::find_if(endDigRevIt, str.rend(), [&](const char &c) {
    if (std::isdigit(c))
      numFound = true;
    return numFound && !std::isdigit(c);
  });

  string numStr(begDigRevIt.base(), endIt + 1);
  stringstream ss(numStr); // define numStr when passing
  int num{};
  ss >> num;
  return num;
}

bool Util::isNum(const string &str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

/// \return
/// True if lock() has been called by a process that is currently running
bool Util::locked() {
  if (!exists(pid_path))
    return false;

  auto pid = read < pid_t > (pid_path);
  return exists(string("/proc/") + to_string(pid));
//      && pid != getpid();
}

void Util::lock() {
  if (locked()) {
    LOG(llvl::error) << "A fancon process is already running, please close it to continue";
    exit(EXIT_FAILURE);
  } else
    write(pid_path, getpid());
}

/// \return True if lock is acquired
bool Util::try_lock() {
  if (locked())
    return false;

  lock();
  return true;
}

string Util::getDir(const string &hwID, DeviceType devType, const bool useSysFS) {
  string d;
  if (devType == DeviceType::fan)
    d = string((useSysFS) ? hwmon_path : fancon_hwmon_path);
  else if (devType == DeviceType::fan_nv)
    d = string(fancon_dir) + nvidia_label;

  return (d += hwID + '/');
}

string Util::getPath(const string &path_pf, const string &hwID, DeviceType devType, const bool useSysFS) {
  return getDir(hwID, devType, useSysFS) + path_pf;
}

string Util::readLine(string path, int nFailed) {
  ifstream ifs(path);
  string line;
  std::getline(ifs, line);
  ifs.close();

  if (ifs.fail()) {
    auto exist = exists(path);
    // Retry read 3 times if file exists, before failing
    if (exist && nFailed <= 3)
      return readLine(path, ++nFailed);

    const char *reason = (exist) ? "filesystem or permission error" : "doesn't exist";
    LOG(llvl::error) << "Failed to read from: " << path << " - " << reason << "; user id " << getuid();
  }

  return line;
}
