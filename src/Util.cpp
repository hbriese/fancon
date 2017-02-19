#include "Util.hpp"

int fanctl::Util::getLastNum(string str) {
  std::reverse(str.begin(), str.end());
  std::stringstream ss(str);

  int num;
  char dc;

  while (ss >> num || !ss.eof()) {
    if (ss.fail()) {
      ss.clear();
      ss >> dc;
    } else    // valid number found
      break;
  }


  // TODO: error check num is set
  return num;
}

bool fanctl::Util::isNum(const string &str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

void fanctl::Util::coutThreadsafe(const string &out) {
  coutLock.lock();
  cout << out;
  coutLock.unlock();
}

const bool fanctl::Util::validIter(const string::iterator &end, std::initializer_list<string::iterator> iterators) {
  for (auto it : iterators)
    if (it == end)
      return false;

  return true;
}

void fanctl::Util::writeSyslogConf() {
  // TODO: run during install instead
  const string syslogConfDir = "/etc/rsyslog.d/";
  const string syslogConf = syslogConfDir + "30-fanctl.conf";

  if (exists(syslogConf))
    return;

  if (!exists(syslogConfDir))
    bfs::create_directory(syslogConfDir);

  ofstream ofs(syslogConf);
  ofs << ":programname, isequal, \"fanctl\" /var/log/fanctl.log" << endl
      << "stop" << endl;
  ofs.close();

  // restart service
  system("/etc/init.d/rsyslog restart");

  if (ofs.fail())
    fanctl::Util::log(LOG_ERR, string("Failed syslog config file: ") + syslogConf);
}

void fanctl::Util::openSyslog(bool debug) {
  int logLevel = (debug) ? LOG_DEBUG : LOG_NOTICE;
  setlogmask(LOG_UPTO(logLevel));
  openlog("fanctl", LOG_PID, LOG_LOCAL1);
  // LOG_NDELAY open before first log
  fanctl::Util::syslog_open = true;
}

void fanctl::Util::closeSyslog() {
  syslog_open = false;
  closelog();
}

void fanctl::Util::log(int logSeverity, const string &message) {
  if (!fanctl::Util::syslog_open)
    throw std::runtime_error("syslog not open");
  syslog(logSeverity, "%s", message.c_str());
}

string fanctl::Util::getPath(const string &path_pf, const string &hwmon_id, const bool prev_failed) {
  string p(hwmon_path + hwmon_id + '/' + path_pf);     // hwmon path

  if (!exists(p) || prev_failed)
    p = string(fanctl_path + hwmon_id + '/' + path_pf);  // fanctl path

  return p;
}

string fanctl::Util::readLine(string path) {
  // check for fanctl config first, if not found (i.e. tests for fan not run), use regular file
  string p_fc(path);
  p_fc.append("_fanctl");

  if (exists(p_fc))
    path = p_fc;

  std::ifstream ifs(path);
  string line;
  std::getline(ifs, line, '\n');

  if (ifs.fail()) {
    // TODO: log, and handle
  }

  return line;
}
