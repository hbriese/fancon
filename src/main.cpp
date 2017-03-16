#include "main.hpp"

void fancon::help() {
  cout << "Usage:" << endl
       << "  fancon <command> [options]" << endl << endl
       << "Available commands (and options <default>):" << endl
       << "-lf list-fans      Lists the UIDs of all fans" << endl
       << "-ls list-sensors   List the UIDs of all temperature sensors" << endl
       << "-wc write-config   Writes missing fan UIDs to " << conf_path << endl
       << "test               Tests the fan characteristic of all fans, required for usage of RPM in " << conf_path
       << endl
       << "  -r retries 4     Number of retries a test does before failure, increase if you think a failing fan can pass!"
       << endl
       << "start              Starts the fancon daemon" << endl
       << "  -f fork          Forks off the parent process" << endl
       << "  -t threads       Ignores \"threads=\" in /etc/fancon.conf and sets maximum number of threads to run"
       << endl
       << "stop               Stops the fancon daemon" << endl
       << "reload             Reloads the fancon daemon" << endl
       << endl << "Global options: " << endl
       << "  -d debug          Write debug level messages to log" << endl
       << "  -q quiet          Only write error level messages to log" << endl;
}

void fancon::writeResumeConf() {
  constexpr const char *gi = ", make a github issue at github.com/hbriese/fancon/issues";
  string sem = string("Unable to write system resume script - undefined behaviour may occur upon resume") + gi;

  // try systemd by default, else try pm-utils
  string sleepDir;
  if (exists("/lib/systemd/"))
    sleepDir = "/lib/systemd/system-sleep/";
  else if (exists("/usr/lib/pm-utils/"))
    sleepDir = "/usr/lib/pm-utils/sleep.d/";
  else {
    cerr << sem << endl;
    return;
  }

  // create sleep directory if absent
  if (!exists(sleepDir))
    bfs::create_directory(sleepDir);

  string p = sleepDir + "fancon";
  string exeP;
  char buff[PATH_MAX];
  ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
  if (len != -1) {
    buff[len] = '\0';
    exeP = string(buff);
  } else {
    cerr << "Invalid fancon executable path length" << gi << endl;
    return;
  }

  ofstream ofs(p);
  ofs << "#!/bin/bash" << endl
      << "case $1 in" << endl
      << "  thaw|resume)" << endl
      << "    " << exeP.c_str() << " reload" << endl
      << "    ;;" << endl
      << "esac" << endl;

  // check for write fail
  if (ofs.fail()) {
    cerr << sem << "; path: " << p << endl;
    return;
  }

  // make script executable
  if (system(string("chmod +x ").append(p).c_str()))
    cerr << "Failed to make resume script executable, please run 'sudo chmod +x " << p << '\'' << endl;
}

string fancon::listFans(SensorController &sc) {
  auto uids = sc.getFans();
  stringstream ss;

  int fcw = 20, scw = 15;
  if (uids.empty())
    ss << "No fans were detected, try running 'sudo sensors-detect' first" << endl;
  else
    ss << setw(fcw) << left << "<Fan UID>" << setw(scw) << left << "<min-max RPM>" << "<min PWM> <max PWM = 255>"
       << endl;

  bool utMessage = false;
  for (auto &uid : uids) {
    stringstream sst;
    sst << uid;
    ss << setw(fcw) << left << sst.str();

    FanPaths p(uid, uid.type);
    string hwID = to_string(uid.hw_id);
    if (p.tested()) {
      sst.str("");
      sst << read<int>(p.rpm_min_pf, hwID, uid.type) << " - " << read<int>(p.rpm_max_pf, hwID, uid.type);
      ss << setw(scw) << left << sst.str() << read<int>(p.pwm_min_pf, hwID, uid.type) << " (or 0)";
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

  ss << ((!uids.empty()) ? "<Sensor UID>" : "No sensors were detected, try running 'sudo sensors-detect' first")
     << endl;

  for (auto &uid : uids) {
    ss << uid;

    // add fan labels after UID
    string label_p = uid.getBasePath() + "_label";
    if (exists(label_p)) {
      string label = Util::readLine(label_p);
      if (!label.empty())
        ss << " (" << label << ")";
    }

    ss << endl;
  }

  return ss.str();
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
  sc.writeConf(conf_path);

  auto fanUIDs = sc.getFans();
  if (fanUIDs.empty())
    cerr << "No fans were detected, try running 'sudo sensors-detect' first" << endl;

  if (!exists(Util::fancon_dir))
    create_directory(Util::fancon_dir);

  cout << "Starting tests. This may take some time (to ensure accurate results)" << endl;
  vector<thread> threads;
  for (auto it = fanUIDs.begin(); it != fanUIDs.end(); ++it) {
    auto dir = Util::getDir(to_string(it->hw_id), it->type);
    if (!exists(dir))
      create_directory(dir);

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
  auto devType = uid.type;
  unique_ptr<FanInterface> fan;

  if (devType == FAN)
    fan.reset(new Fan(uid));
  else if (devType == FAN_NVIDIA) {
#ifdef FANCON_NVIDIA_SUPPORT
    fan.reset(new FanNV(uid));
#else
    LOG(llvl::warning) << "Trying to test NVIDIA fan, but NVIDIA support is disabled";
    return;
#endif
  }

  FanTestResult res;
  stringstream ss;
  for (; retries > 0; --retries) {
    res = fan->test();

    if (res.testable() && res.valid()) {
      FanInterface::writeTestResult(uid, res, devType);

      ss << uid << " passed" << endl;
      break;
    } else if (devType == FAN_NVIDIA)
      LOG(llvl::error)
        << "NVIDIA manual fan control CoolBit is not set."
        << "Please run 'sudo nvidia-xconfig --cool-bits=4', restart your X server (or reboot) and retry test";
  }

  if (retries <= 0)
    ss << uid << ((res.testable()) ? " results are invalid, consider running with more --retries (default is 4)"
                                   : " cannot be tested, driver unresponsive") << endl;
  coutThreadsafe(ss.str());
}

void fancon::handleSignal(int sig) {
  switch (sig) {
  case SIGTERM:
  case SIGINT:
  case SIGABRT: fancon::daemon_state = DaemonState::STOP;
    break;
  case SIGHUP: fancon::daemon_state = DaemonState::RELOAD;
    break;
  default: LOG(llvl::warning) << "Unknown signal caught: " << sig;
  }
}

void fancon::start(SensorController &sc, const bool fork_) {
  if (fork_) {
    // Fork off the parent process
    pid_t pid = fork();
    auto exitParent = [](pid_t &p) {
      if (p > 0)
        exit(EXIT_SUCCESS);
      else if (p < 0) {
        LOG(llvl::fatal) << "Failed to fork off parent";
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
      LOG(llvl::fatal) << "Failed to fork off parent";
      exit(EXIT_FAILURE);
    }

    // fork again
    pid = fork();
    exitParent(pid);

    // update pid file to reflect forks
    write<pid_t>(Util::pid_file, getpid());
  }

  // Set file mode
  umask(0);

  // Change working directory
  if (chdir("/") < 0)
    LOG(llvl::warning) << "Failed to set working directory";

  auto tsParents = sc.readConf(conf_path);
  if (tsParents.empty()) {
    LOG(llvl::info) << "No fan configurations found, exiting fancond. See 'fancon help'";
    return;
  }

  // set daemon state
  fancon::daemon_state = DaemonState::RUN;

  auto nThreads = sc.conf.threads;
  auto nTasks = tsParents.size();
  // allocate tasks to threads
  auto threadTasks = fancon::getThreadTasks(nThreads, nTasks);
  nThreads = static_cast<uint>(threadTasks.size());

  vector<thread> threads;
  auto begIt = tsParents.begin();
  for (uint i = 0; i < nThreads; ++i) {
    auto endIt = next(begIt, threadTasks[i]);

    // main loop, stopped when signal is sent to signal handler
    threads.emplace_back([](vector<unique_ptr<SensorParentInterface>>::iterator first,
                            vector<unique_ptr<SensorParentInterface>>::iterator last,
                            DaemonState &state, uint &updateTime) {
      while (state == fancon::DaemonState::RUN) {
        for (auto tspIt = first; tspIt != last; ++tspIt)
          if ((*tspIt)->update())   // update fans if temp changed
            for (auto &fp : (*tspIt)->fans)
              fp->update((*tspIt)->temp);

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
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGABRT, &act, NULL);
  sigaction(SIGHUP, &act, NULL);

  LOG(llvl::debug) << "fancond started";
  for (auto &t : threads)
    if (t.joinable())
      t.join();
    else
      LOG(llvl::debug) << "Unable to join thread ID: " << t.get_id();

  // re-run this function if reload
  if (fancon::daemon_state == DaemonState::RELOAD) {
    LOG(llvl::info) << "Reloading fancond";
    return fancon::start(sc, fork_);
  }

  return;
}

void fancon::send(DaemonState state) {
  // use kill() to send signal to the PID
  if (Util::locked())
    kill(read<pid_t>(Util::pid_file), static_cast<int>(state));
  else if (state == RELOAD)
    LOG(llvl::info) << "Cannot reload: fancond is not running";
}

int main(int argc, char *argv[]) {
  const bool root = (getuid() == 0);

  if (!exists(Util::fancon_dir) || !exists(conf_path)) {    // TODO: run more broadly
    create_directory(Util::fancon_dir);
    const char *bold = "\033[1m", *red = "\033[31m", *resetF = "\033[0m";
    string
        m("First use: run 'sudo fancon test && sudo fancon -lf -ls', then configure fan profiles in /etc/fancon.conf");
    cout << bold << setw(m.size()) << std::setfill('-') << left << '-' << endl
         << red << m << resetF << endl
         << bold << setw(m.size()) << left << '-' << resetF << endl << endl;
    if (root)
      fancon::writeResumeConf();
  }

  vector<string> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(string(argv[i]));

  fancon::Command help("help", false, false), start("start"), stop("stop"), reload("reload"),
      list_fans("list-fans", true), list_sensors("list-sensors", true),
      test("test"), write_config("write-config", true);
  vector<reference_wrapper<fancon::Command>> commands
      {help, start, stop, reload, list_fans, list_sensors, test, write_config};

  fancon::Option debug("debug"), quiet("quiet"), threads("threads", true), fork("fork"), retries("retries", true);
  vector<reference_wrapper<fancon::Option>> options
      {debug, quiet, threads, fork, retries};

  for (auto a = args.begin(); a != args.end(); ++a) {
    if (a->empty())
      continue;

    // remove all preceding '-'
    a->erase(a->begin(), find_if(a->begin(), a->end(), [](const char &c) { return c != '-'; }));

    // convert to lower case
    std::transform(a->begin(), a->end(), a->begin(), ::tolower);

    // search commands
    auto cIt = find_if(commands.begin(), commands.end(), [&a](auto &c) { return c.get() == *a; });
    if (cIt != commands.end()) {
      cIt->get().called = true;
      continue;
    }

    // search options
    auto oIt = find_if(options.begin(), options.end(), [&a](auto &o) { return o.get() == *a; });
    if (oIt != options.end()) {
      auto &o = oIt->get();

      if (o.require_root && !root) {  // check for root requirement
        cerr << "Please run with sudo, or as root for command: " << o.name << endl;
        break;
      }

      if (o.has_value) {  // handle value, if required
        // validate the next argument, and store value
        auto na = next(a);

        if (na != args.end() && Util::isNum(*na)) {  // valid
          o.val = static_cast<uint>(std::stoul(*na));
          o.called = true;
          ++a;  // skip na iterator in current args loop
        } else
          cerr << "Option '" << *a << "' required a value (>= 0)" << endl;

      } else
        o.called = true;

      continue;
    }

    cerr << "Unknown argument: " << *a << endl;
  }

  // set log level
  if (debug.called)
    fancon::log::setLevel(llvl::trace);
  else if (quiet.called)
    fancon::log::setLevel(llvl::error);

  // execute called commands with called options
  if (help.called || args.empty()) // execute help() if no commands are given
    fancon::help();
  else if (stop.called)
    fancon::send(DaemonState::STOP);
  else if (reload.called)
    fancon::send(DaemonState::RELOAD);

  SensorController sc(threads.val);

  if (list_fans.called)
    coutThreadsafe(fancon::listFans(sc));
  if (list_sensors.called)
    coutThreadsafe(fancon::listSensors(sc));
  if (write_config.called || !exists(conf_path)) {
    fancon::writeResumeConf();  // ensure resume scripts exist
    sc.writeConf(conf_path);
  }

  // Allow only one process past this point
  if (!Util::locked()) {
    Util::lock();

    if (start.called)
      fancon::start(sc, fork.called);
    else if (test.called) {
      uint nRetries = (retries.called && retries.val > 0) ? retries.val : 4;   // default 4 retries
      fancon::test(sc, nRetries, (threads.called && threads.val == 1));
    }
  } else if (start.called || test.called)
    cout << "A fancon process is already running." << endl;

  return 0;
}