#include "main.hpp"

string fancon::help() {
  stringstream ss;
  ss << "Usage:" << endl
     << "  fancon <command> [options]" << endl << endl
     << "Available commands (and options <default>):" << endl
     << "-lf list-fans        Lists the UIDs of all fans" << endl
     << "-ls list-sensors     List the UIDs of all temperature sensors" << endl
     << "-wc write-config     Writes missing fan UIDs to " << fancon::conf_path << endl
     << "test                 Tests the fan characteristic of all fans, required for usage of RPM in "
     << fancon::conf_path << endl
     << "  -r retries 4       Number of retries a test does before failure, increase if you think a failing fan can pass!"
     << endl
     << "start                Starts the fancon daemon" << endl
     << "  -f fork            Forks off the parent process" << endl
     << "  -t threads         Ignores 'threads=' in /etc/fancon.conf and sets maximum number of threads to run" << endl
     << "stop                 Stops the fancon daemon" << endl
     << "reload               Reloads the fancon daemon" << endl
     << endl << "Global options: " << endl
     << "  -d debug           Writes debug level messages to syslog" << endl
     << endl;

  return ss.str();
}

void fancon::firstTimeSetup() {
  bfs::create_directory(fancon::Util::fancon_dir);

  string rsyslogDir("/etc/rsyslog.d/");
  string p = rsyslogDir + "30-fancon.conf";

  if (exists(rsyslogDir) && !exists(p)) {
    string logP("/var/log/fancon.log");
    ofstream ofs(p);
    ofs << ":programname, isequal, \"fancon\" " << logP << endl
        << "stop" << endl;
    if (ofs.fail())
      cerr << "Failed to write a fancon syslog config, logs will be written directly to " << logP
           << ", please make a github issue: " << p << endl;

    // restart service
    if (system("/etc/init.d/rsyslog restart"))
      cerr << "Failed to restart rsyslog, /var/log/syslog will be used until reboot, or rsyslog restarted" << endl;
  }

  string pmSleepDir("/etc/pm/sleep.d/");
  const char *pmScriptErr = "Failed to write pm script, fancon may not work on wakeup, please make a github issue";
  if (!exists(pmSleepDir))
    cerr << pmScriptErr << endl;

  p = pmSleepDir + "fancon";
  if (exists(pmSleepDir) && !exists(p)) {   // TODO: remove exists(pmSleepDir) double check
    // get executable path
    string exeP;
    char buff[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
    if (len != -1) {
      buff[len] = '\0';
      exeP = string(buff);
    } else {
      cerr << "Invalid fancon path length, please make a github issue" << endl;
      exit(1);
    }

    ofstream ofs(p);
    ofs << "#!/bin/bash" << endl
        << "case $1 in" << endl
        << "resume)" << endl
        << "    sleep 3" << endl
        << "    " << exeP.c_str() << " reload" << endl
        << "    ;;" << endl
        << "esac" << endl;
    if (ofs.fail())
      cerr << pmScriptErr << p << endl;
  }
}

string fancon::listFans(SensorController &sc) {
  auto uids = sc.getFansAll();
  stringstream ss;

  int fcw = 20, scw = 15;
  if (uids.empty())
    ss << "No fans were detected, try running 'sudo sensors-detect' first" << endl;
  else
    ss << setw(fcw) << left << "<Fan UID>" << setw(scw) << left << "<min-max RPM>" << "<min PWM>" << endl;

  bool utMessage = false;
  for (auto &uid : uids) {
    stringstream sst;
    sst << uid;
    ss << setw(20) << left << sst.str();

    FanInterface f(uid);
    if (f.tested) {
      sst.str("");
      sst << f.rpm_min << " - " << f.rpm_max;
      ss << setw(scw) << left << sst.str() << f.pwm_min << " (or 0)";
    } else
      utMessage = true;

    ss << endl;
  }

  if (utMessage)
    ss << endl << "Note: un-tested fans have no values" << endl;

  return ss.str();
}

string fancon::listSensors(SensorController &sc) {
  auto uids = sc.getSensors();
  stringstream ss;

  for (auto &uid : uids) {
    ss << uid;

    // add fan labels after UID
    string label_p = uid.getBasePath() + "_label";
    if (exists(label_p)) {
      string label = fancon::Util::readLine(label_p);
      if (!label.empty())
        ss << " (" << label << ")";
    }

    ss << endl;
  }

  return ss.str();
}

bool fancon::pidExists(pid_t pid) {
  string pid_path = string("/proc/") + to_string(pid);
  return exists(pid_path);
}

void fancon::writeLock() {
  if (exists(fancon::pid_file)
      && fancon::pidExists(fancon::Util::read<pid_t>(fancon::pid_file))) {  // fancon process running
    cout << "Error: a fancon process is already running, please close it to continue" << endl;
    exit(1);
  } else
    write<pid_t>(fancon::pid_file, getpid());
}

vector<ulong> fancon::getThreadTasks(uint nThreads, ulong nTasks) {
  if (nTasks < nThreads)
    nThreads = static_cast<uint>(nTasks);

  // allocate tasks to threads
  ulong tpt = nTasks / nThreads;   // tasks per thread
  auto tRem = nTasks % nThreads;
  // allocate threads with regular amount of tasks
  vector<ulong> threadTasks(nThreads - tRem, tpt);
  if (tRem)   // insert threads that have extra tasks
    threadTasks.insert(threadTasks.end(), tRem, tpt + 1);

  return threadTasks;
}

void fancon::test(SensorController &sc, uint testRetries, bool singleThread) {
  fancon::writeLock();
  sc.writeConf(fancon::conf_path);

  auto fanUIDs = sc.getFansAll();
  if (fanUIDs.empty())
    cerr << "No fans were detected, try running 'sudo sensors-detect' first" << endl;

  if (!exists(Util::fancon_dir))
    bfs::create_directory(Util::fancon_dir);

  cout << "Starting tests. This may take some time (to ensure accurate results)" << endl;
  vector<thread> threads;
  for (auto it = fanUIDs.begin(); it != fanUIDs.end(); ++it) {
    auto dir = Util::getDir(to_string(it->hwID), it->type);
    if (!exists(dir))
      bfs::create_directory(dir);

    threads.push_back(thread(&fancon::testUID, std::ref(*it), testRetries));

    if (singleThread) {
      threads.back().join();
      threads.pop_back();
    }
  }

  // rejoin threads
  for (auto &t : threads)
    if (t.joinable())
      t.join();
    else
      t.detach();
}

void fancon::testUID(UID &uid, uint retries) {
  stringstream ss;
  FanTestResult res;
  auto devType = uid.type;

  for (; retries > 0; --retries) {
    res = (devType == FAN_NVIDIA) ? FanNVIDIA::test(uid) : Fan::test(uid);

    if (res.testable() && res.valid()) {
      FanInterface::writeTestResult(uid, res, devType);

      ss << uid << " passed" << endl;
      break;
    }
  }

  if (retries <= 0 && res.testable())
    ss << uid << " results are invalid, consider running with more --retries (default is 4)" << endl;
  else if (retries <= 0 && !res.testable())
    ss << uid << " cannot be tested, driver unresponsive" << endl;

  coutThreadsafe(ss.str());
}

void fancon::handleSignal(int sig) {
  switch (sig) {
  case (int) DaemonState::RELOAD:
  case (int) DaemonState::STOP: fancon::daemon_state = (DaemonState) sig;
    break;
  default:log(LOG_NOTICE, string("Unknown signal caught") + to_string(sig));
  }
}

void fancon::start(SensorController &sc, const bool fork_, uint nThreads, const bool writeLock) {
  if (writeLock)
    fancon::writeLock();

  if (fork_) {
    // Fork off the parent process
    pid_t pid = fork();
    auto exitParent = [](pid_t &p) {
      if (p > 0)
        exit(EXIT_SUCCESS);
      else if (p < 0) {
        log(LOG_ERR, "Failed to fork off parent");
        exit(EXIT_FAILURE);
      }
    };
    exitParent(pid);

    // redirect standard file descriptors to /dev/null
    stdin = freopen("/dev/null", "r", stdin);
    stdout = freopen("/dev/null", "w+", stdout);
    stderr = freopen("/dev/null", "w+", stderr);

    // Create a new session for the child
    if ((pid = setsid()) < 0) {
      log(LOG_ERR, "Failed to set session id for the forked fancond thread");
      exit(EXIT_FAILURE);
    }

    // fork again
    pid = fork();
    exitParent(pid);

    // update pid file to reflect forks
    write<pid_t>(fancon::pid_file, getpid());
  }

  // Set file mode
  umask(0);

  // Change working directory
  if (chdir("/") < 0) {
    log(LOG_ERR, "Failed to set working directory");
    exit(EXIT_FAILURE);
  }

  auto tsParents = sc.readConf(fancon::conf_path);
  if (tsParents.empty()) {
    log(LOG_NOTICE, "No fan configurations found, exiting fancond. See 'fancon help'");
    return;
  }

  // set daemon state
  fancon::daemon_state = DaemonState::RUN;

  // use nThreads if provided, else from the config
  if (nThreads == 0)
    nThreads = sc.conf.threads;
  auto nTasks = tsParents.size();
  // allocate tasks to threads
  auto threadTasks = fancon::getThreadTasks(nThreads, nTasks);
  nThreads = static_cast<uint>(threadTasks.size());

  vector<thread> threads;
  auto begIt = tsParents.begin();
  for (uint i = 0; i < nThreads; ++i) {
    auto endIt = next(begIt, threadTasks[i]);

    // main loop, stopped when signal is sent to signal handler
    threads.emplace_back([](vector<unique_ptr<TSParent>>::iterator first,
                            vector<unique_ptr<TSParent>>::iterator last, DaemonState &state, uint &updateTime) {
      while (state == fancon::DaemonState::RUN) {
        for (auto tspIt = first; tspIt != last; ++tspIt) {
          // update fans if temp changed
          if ((*tspIt)->update()) {
            for (auto &fp : (*tspIt)->fans)
              fp->update((*tspIt)->temp);

            for (auto &fp : (*tspIt)->fansNVIDIA)
              fp->update((*tspIt)->temp);
          }
        }

        sleep(updateTime);
      }
    }, begIt, endIt, ref(fancon::daemon_state), ref(sc.conf.update_time_s));

    begIt = endIt;
  }

  // handle SIGINT and SIGHUP signals
  struct sigaction act;
  act.sa_handler = fancon::handleSignal;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGHUP, &act, NULL);

  log(LOG_DEBUG, "fancond started");
  for (auto &t : threads)
    if (t.joinable())
      t.join();
    else
      log(LOG_DEBUG, "Unable to join thread");

  // re-run this function if reload
  if (fancon::daemon_state == DaemonState::RELOAD) {
    log(LOG_NOTICE, "Reloading fancond");
    return fancon::start(sc, fork_, nThreads, false);  // writeLock = false
  }

  return;
}

void fancon::send(DaemonState state) {
  // use kill() to send signals to the PID
  pid_t pid = read<pid_t>(fancon::pid_file, 0);
  if (pidExists(pid))
    kill(pid, (int) state);
  else
    cerr << "Error: fancond is not running" << endl;
}

int main(int argc, char *argv[]) {
  bool root = getuid() == 0;

  if (!exists(fancon::Util::fancon_dir)) {
    const char *bold = "\033[1m", *red = "\033[31m", *reset = "\033[0m";
    string m("First use: please run 'fancon test', then configure fan profiles in /etc/fancon.conf");
    cout << bold << setw(m.size()) << std::setfill('-') << left << '-' << endl;
    cout << red << m << reset << endl;
    cout << bold << setw(m.size()) << left << '-' << reset << endl << endl;
    if (root)
      fancon::firstTimeSetup();
  }

  vector<string> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(string(argv[i]));

  fancon::Command help("help", false), start("start"), stop("stop"), reload("reload"),
      list_fans("list-fans", true, true), list_sensors("list-sensors", false, true),
      test("test"), write_config("write-config", true, true);
  vector<reference_wrapper<fancon::Command>> commands
      {help, start, stop, reload, list_fans, list_sensors, test, write_config};

  fancon::Option debug("debug"), threads("threads", true), fork("fork"), retries("retries", true);
  vector<reference_wrapper<fancon::Option>> options
      {debug, threads, fork, retries};

  for (auto it = args.begin(); it != args.end(); ++it) {
    auto &a = *it;
    if (a.empty())
      continue;

    // remove preceding '-'s
    while (a.front() == '-')
      a.erase(a.begin());

    // convert to lower case
    std::transform(a.begin(), a.end(), a.begin(), ::tolower);

    bool found = false;
    // set param or arg to called if equal
    for (auto &c : commands)
      if (c.get() == a) {
        c.get().called = found = true;
        break;
      }

    if (!found) {
      for (auto &o : options) {
        if (o.get() == a) {
          // check for root
          if (o.get().require_root && !root) {
            cerr << "Please run with sudo, or as root for command: " << o.get().name << endl;
            continue;
          }

          // if option has value, check valid and set
          if (o.get().has_value) {
            auto ni = next(it);
            if (ni != args.end() && fancon::Util::isNum(*ni)) {

              o.get().val = static_cast<uint>(std::stoul(*ni));
              o.get().called = found = true;
              ++it;   // skip next arg

            } else {
              string valOut = (ni != args.end()) ? (string(" (") + *ni + ") ") : " ";
              cerr << "No valid value" << valOut << "given for option: " << o.get().name << endl;
            }
          } else
            o.get().called = found = true;

          break;
        }
      }
    }

    if (!found)
      cerr << "Unknown argument: " << a << endl;
  }

  SensorController sc(debug.called, threads.val);

  // execute called commands with called options
  if (help.called || args.empty()) // execute help() if no commands are given
    coutThreadsafe(fancon::help());

  if (start.called)
    fancon::start(sc, fork.called);
  else if (stop.called)
    fancon::send(DaemonState::STOP);
  else if (reload.called)
    fancon::send(DaemonState::RELOAD);
  else if (test.called) {
    uint nRetries = (retries.called && retries.val > 0) ? retries.val : 4;   // default 4 retries
    fancon::test(sc, nRetries, (threads.called && threads.val == 1));
  }

  if (list_fans.called)
    coutThreadsafe(fancon::listFans(sc));
  if (list_sensors.called)
    coutThreadsafe(fancon::listSensors(sc));
  if (write_config.called || !exists(fancon::conf_path))
    sc.writeConf(fancon::conf_path);

  return 0;
}