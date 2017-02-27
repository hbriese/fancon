#include "Util.hpp"

using namespace fancon;

int Util::getLastNum(const string &str) {
  auto endDigRevIt = std::find_if(str.rbegin(), str.rend(), [](const char &c) { return std::isdigit(c); });
  auto endIt = (endDigRevIt + 1).base();

  bool numFound = false;
  auto begDigRevIt = std::find_if(endDigRevIt, str.rend(), [&numFound](const char &c) {
    if (std::isdigit(c))
      numFound = true;
    return numFound && !std::isdigit(c);
  });

  string numStr(begDigRevIt.base(), endIt + 1);
  std::stringstream ss(numStr);
  int num{};
  ss >> num;
  return num;
}

bool Util::isNum(const string &str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

void Util::coutThreadsafe(const string &out) {
  coutLock.lock();
  cout << out;
  coutLock.unlock();
}

bool Util::validIter(const string::iterator &end, std::initializer_list<string::iterator> iterators) {
  for (auto it : iterators)
    if (it == end)
      return false;

  return true;
}

string Util::getDir(const string &hwID, DeviceType devType, const bool useSysFS) {
  string d;
  if (devType == DeviceType::FAN)
    d = string((useSysFS) ? hwmon_path : fancon_path) + hwID;
  else if (devType == FAN_NVIDIA)
    d = string(fancon_dir) + nvidia_label;
  else
    LOG(severity_level::debug) << "SysFS can only be used for DeviceType::FAN";

  return (d += '/');
}

string Util::getPath(const string &path_pf, const string &hwID, DeviceType devType, const bool useSysFS) {
  return getDir(hwID, devType, useSysFS) + path_pf;
}

string Util::readLine(string path) {
  // check for fancon config first, if not found (i.e. tests for fan not run), use regular file
  string p_fc(path);
  p_fc.append("_fancon");

  if (exists(p_fc))
    path = p_fc;

  std::ifstream ifs(path);
  string line;
  std::getline(ifs, line);

  if (ifs.fail())
    LOG(severity_level::error) << "Failed to read from: " << path;

  return line;
}
