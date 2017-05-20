#include "Util.hpp"
#include <boost/filesystem/operations.hpp>

namespace Util = fancon::Util;

fancon::DeviceType fancon::operator|(fancon::DeviceType lhs,
                                     fancon::DeviceType rhs) {
  return static_cast<fancon::DeviceType>(
      static_cast<std::underlying_type<fancon::DeviceType>::type>(lhs) |
      static_cast<std::underlying_type<fancon::DeviceType>::type>(rhs));
}

fancon::DeviceType fancon::operator&(fancon::DeviceType lhs,
                                     fancon::DeviceType rhs) {
  return static_cast<fancon::DeviceType>(
      static_cast<std::underlying_type<fancon::DeviceType>::type>(lhs) &
      static_cast<std::underlying_type<fancon::DeviceType>::type>(rhs));
}

int Util::lastNum(const string &str) {
  auto endRevIt = std::find_if(str.rbegin(), str.rend(),
                               [](const char &c) { return std::isdigit(c); });

  bool numFound = false;
  auto begRevIt = std::find_if_not(endRevIt, str.rend(), [&](const char &c) {
    if (std::isdigit(c))
      return (numFound = true);

    return !numFound;
  });

  stringstream ss(string(begRevIt.base(), endRevIt.base()));
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

  const auto pid = read<pid_t>(pid_path);
  return exists(string("/proc/") + to_string(pid)) && pid != getpid();
}

void Util::lock() {
  if (locked()) {
    LOG(llvl::error)
        << "A fancon process is already running, please close it to continue";
    exit(EXIT_FAILURE);
  }

  write(pid_path, getpid());
}

/// \return True if lock is acquired
bool Util::try_lock() {
  if (locked())
    return false;

  lock();
  return true;
}

string Util::getDir(const string &hwID, DeviceType devType, const bool sysFS) {
  string d;
  if (devType == DeviceType::fan)
    d = string((sysFS) ? hwmon_path : fancon_hwmon_path);
  else if (devType == DeviceType::fan_nv)
    d = string(fancon_dir) + nvidia_label;

  return (d += hwID + '/');
}

string Util::getPath(const string &path_pf, const string &hwID,
                     DeviceType devType, const bool sysFS) {
  return getDir(hwID, devType, sysFS) + path_pf;
}

string Util::readLine(string path, int nFailed) {
  ifstream ifs(path);
  string line;
  std::getline(ifs, line);
  ifs.close();

  if (!ifs) {
    auto exist = exists(path);
    // Retry read 3 times if file exists, before failing
    if (exist && nFailed <= 3)
      return readLine(path, ++nFailed);

    const char *reason =
        (exist) ? "filesystem or permission error" : "doesn't exist";
    LOG(llvl::error) << "Failed to read from: " << path << " - " << reason
                     << "; user id " << getuid();
  }

  return line;
}
