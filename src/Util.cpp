#include "Util.hpp"

int fancon::Util::getLastNum(string str) {
  std::reverse(str.begin(), str.end());
  std::stringstream ss(str);

  int num = -1;
  char dc;

  while (ss >> num || !ss.eof()) {
    if (ss.fail()) {
      ss.clear();
      ss >> dc;
    } else    // valid number found
      break;
  }

  if (num < 0)
    log(LOG_ERR, string("Failed to get hwmon ID from '") + str + "', please submit a github issue.");

  return num;
}

bool fancon::Util::isNum(const string &str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

void fancon::Util::coutThreadsafe(const string &out) {
  coutLock.lock();
  cout << out;
  coutLock.unlock();
}

bool fancon::Util::validIter(const string::iterator &end, std::initializer_list<string::iterator> iterators) {
  for (auto it : iterators)
    if (it == end)
      return false;

  return true;
}

void fancon::Util::openSyslog(bool debug) {
  int logLevel = (debug) ? LOG_DEBUG : LOG_NOTICE;
  setlogmask(LOG_UPTO(logLevel));
  openlog("fancon", LOG_PID, LOG_LOCAL1);
  // LOG_NDELAY open before first log
}

void fancon::Util::closeSyslog() {
  closelog();
}

string fancon::Util::readLine(string path) {
  // check for fancon config first, if not found (i.e. tests for fan not run), use regular file
  string p_fc(path);
  p_fc.append("_fancon");

  if (exists(p_fc))
    path = p_fc;

  std::ifstream ifs(path);
  string line;
  std::getline(ifs, line);

  if (ifs.fail())
    log(LOG_ERR, string("Failed to read from: ") + path);

  return line;
}
