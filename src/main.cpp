#include "main.hpp"

string fancon::help() {
  stringstream ss;
  ss << "Usage:" << endl
     << "  fancon <command> [options]" << endl << endl
     << "Available commands (and options <default>):" << endl
     << "-lf list-fans         Lists the UIDs of all fans" << endl
     << "-ls list-sensors      List the UIDs of all temperature sensors" << endl
     << "-wc write-config      Writes missing fan UIDs to " << fancon::conf_path << endl
     << "test                 Tests the fan characteristic of all fans, required for usage of RPM in "
     << fancon::conf_path << endl
     << "  -r retries 4       Number of retries a test does before failure, increase if you think a failing fan can pass!"
     << endl
     << "  -p profile         Writes a PWM to RPM profile for the fan, not recommended unless submitting a bug report"
     << endl
     << "start                Starts the fancon daemon" << endl
     << "  -f fork            Forks off the parent process, not recommended for systemd usage" << endl
     << "  -t threads         Ignores 'threads=' in /etc/fancon.conf and sets maximum number of threads to run" << endl
     << "stop                 Stops the fancon daemon" << endl
     << "reload               Reloads the fancon daemon" << endl
     << endl << "Global options: " << endl
     << "  -d debug            Writes debug level messages to syslog" << endl
     << endl;

  return ss.str();
}

string fancon::listFans(SensorController &sc) {
  auto uids = sc.getUIDs(Fan::path_pf);
  stringstream ss;

  for (auto &uid : uids)
    ss << uid << endl;

  return ss.str();
}

string fancon::listSensors(SensorController &sc) {
  auto uids = sc.getUIDs(TemperatureSensor::path_pf);
  stringstream ss;

  for (auto &uid : uids) {
    ss << uid;

    // add fan labels after UID
    string label_p(uid.getBasePath());
    string label = fancon::Util::readLine(label_p.append("_label"));
    if (!label.empty())
      ss << " (" << label << ")";

    ss << endl;
  }

  return ss.str();
}

bool fancon::pidExists(pid_t pid) {
  string pid_path = string("/proc/") + to_string(pid);
  return exists(pid_path);
}

void fancon::writeLock() {
  pid_t pid = fancon::Util::read<pid_t>(fancon::pid_file, 0);
  if (fancon::pidExists(pid)) {  // fancon process running
    std::cerr << "Error: a fancon process is already running, please close it to continue" << endl;
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

void fancon::test(SensorController &sc, const bool profileFans, uint testRetries, bool singleThread) {
  fancon::writeLock();

  auto fanUIDs = sc.getUIDs(fancon::Fan::path_pf);
  if (!exists(Util::fancon_dir))
    bfs::create_directory(Util::fancon_dir);

  vector<thread> threads;
  for (auto it = fanUIDs.begin(); it != fanUIDs.end(); ++it) {
    if (it->hwmon_id != std::prev(it)->hwmon_id)
      bfs::create_directory(string(fancon::Util::fancon_path) + to_string(it->hwmon_id));

    threads.push_back(thread(&fancon::testUID, std::ref(*it), profileFans, testRetries));

    // run single at a time if debug mode enabled
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

void fancon::testUID(UID &uid, const bool profileFans, uint retries) {
  stringstream ss;
  TestResult res;
  for (; retries > 0; --retries) {
    res = Fan::test(uid, profileFans);

    if (res.valid()) {
      Fan::writeTestResult(uid, res);
      ss << uid << " passed" << endl;
      break;
    }
  }

  if (retries <= 0 && res.testable())
    ss << uid << " results are invalid, consider running with more retries (--retries)" << endl;
  else if (retries <= 0 && !res.testable())
    ss << uid << " cannot be tested, driver unresponsive" << endl;

  coutThreadsafe(ss.str());
  log(LOG_DEBUG, ss.str());
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
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w+", stdout);
    freopen("/dev/null", "w+", stderr);

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
  if (tsParents.empty())
    return;

  // set daemon state
  fancon::daemon_state = DaemonState::RUN;

  // use nThreads if provided, else from the config
  if (nThreads == 0)
    nThreads = sc.conf.threads;
  auto nTasks = tsParents.size();
  // allocate tasks to threads
  auto threadTasks = fancon::getThreadTasks(nThreads, nTasks);
  nThreads = static_cast<uint>(threadTasks.size());

  vector<future<void>> threadFutures;
  auto begIt = tsParents.begin();
  for (uint i = 0; i < nThreads; ++i) {
    auto endIt = next(begIt, threadTasks[i]);
    threadFutures.emplace_back(std::async(std::launch::async,
                                          &SensorController::run, ref(sc), begIt, endIt, ref(fancon::daemon_state)
    ));
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
  while (fancon::daemon_state == DaemonState::RUN)
    sleep(1);

  // re-run this function if reload
  if (fancon::daemon_state == DaemonState::RELOAD)
    fancon::start(sc, fork_, nThreads, false);  // writeLock = false

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
  vector<string> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(string(argv[i]));

  // for first time setup
  fancon::Util::writeSyslogConf();

  fancon::Command help("help"), start("start"), stop("stop"), reload("reload"),
      list_fans("list-fans", true), list_sensors("list-sensors", true),
      test("test"), write_config("write-config", true);
  vector<reference_wrapper<fancon::Command>> commands
      {help, start, stop, reload, list_fans, list_sensors, test, write_config};

  // TODO: add options: new/exclude fan testing regular expression option
  fancon::Option debug("debug", true), threads("threads", true), fork("fork", true),
      profiler("profiler", true), retries("retries", true, true);
  vector<reference_wrapper<fancon::Option>> options
      {debug, threads, fork, profiler, retries};

  for (auto it = args.begin(); it != args.end(); ++it) {
//  for (auto &a : args) {
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
      for (auto &o : options)
        if (o.get() == a) {
          // if option has value, check valid and set
          if (o.get().has_value) {
            auto ni = next(it);
            if (ni != args.end() && fancon::Util::isNum(*ni)) {

              o.get().val = (uint) std::stoul(*ni);
              o.get().called = found = true;
              ++it;   // skip next arg

            } else {
              string valOut = (ni != args.end()) ? (string(" (") + *ni + ") ") : "";
              cerr << "No valid value" << valOut << " given for option: " << o.get().name << endl;
            }
          } else
            o.get().called = found = true;

          break;
        }
    }
    if (!found)
      cerr << "Unknown argument: " << a << endl;
  }

  SensorController sc(debug.called);

  if (threads.called)
    sc.conf.threads = static_cast<uint>(threads.val);

  // execute called commands with called options
  if (help.called || args.empty()) // execute help() if no commands are given
    coutThreadsafe(fancon::help());
  else if (start.called)
    fancon::start(sc, fork.called);
  else if (stop.called)
    fancon::send(DaemonState::STOP);
  else if (reload.called)
    fancon::send(DaemonState::RELOAD);
  else if (list_fans.called)
    coutThreadsafe(fancon::listFans(sc));
  else if (list_sensors.called)
    coutThreadsafe(fancon::listSensors(sc));
  else if (test.called) {
    uint nRetries = (retries.called && retries.val > 0) ? retries.val : 4;   // default 4 retries
    fancon::test(sc, profiler.called, nRetries);
  } else if (write_config.called)
    sc.writeConf(fancon::conf_path);

  return 0;
}