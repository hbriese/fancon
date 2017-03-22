#include "main.hpp"

void fancon::help() {
  LOG(llvl::info)
    << "Usage:\n"
    << "  fancon <command> [options]\n\n"
    << "Available commands (and options <default>):\n"
    << "-lf list-fans      Lists the UIDs of all fans\n"
    << "-ls list-sensors   List the UIDs of all temperature sensors\n"
    << "-wc write-config   Writes missing fan UIDs to " << conf_path << '\n'
    << "test               Tests the fan characteristic of all fans, required for usage of RPM in " << conf_path
    << '\n'
    << "  -r retries 4     Number of retries a test does before failure, increase if you think a failing fan can pass!\n"
    << "start              Starts the fancon daemon\n"
    << "  -f fork          Forks off the parent process\n"
    << "  -t threads       Ignores \"threads=\" in /etc/fancon.conf and sets maximum number of threads to run\n"
    << "stop               Stops the fancon daemon\n"
    << "reload             Reloads the fancon daemon\n\n"
    << "Global options:\n"
    << "  -v verbose       Output all messages\n"
    << "  -q quiet         Output only error or fatal messages\n\n"
    << "For NVIDIA GPU fan control, install the following packages (library): "
    << "libx11-6 (libX11.so); libxnvctrl0 (libXNVCtrl.so)'";
}

void fancon::listFans(Controller &sc) {
  auto uids = sc.getFans();
  stringstream ss;

  int fcw = 20, scw = 15;
  if (uids.empty())
    ss << "No fans were detected, try running 'sudo sensors-detect' first\n";
  else
    ss << setw(fcw) << left << "<Fan UID>" << setw(scw) << left << "<min-max RPM>" << "<min PWM> <max PWM = 255>\n";

  bool untested = false;
  for (const auto &uid : uids) {
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
      untested = true;

    ss << '\n';
  }

  if (untested)
    ss << "\nNote: un-tested fans have no values";

  LOG(llvl::info) << ss.rdbuf();
}

void fancon::listSensors(Controller &sc) {
  auto uids = sc.getSensors();
  stringstream ss;

  ss << ((!uids.empty()) ? "<Sensor UID>" : "No sensors were detected, try running `sudo sensors-detect` first");

  for (const auto &uid : uids) {
    ss << '\n' << uid;

    // add fan labels after UID
    string label_p = uid.getBasePath() + "_label";
    if (exists(label_p)) {
      string label = Util::readLine(label_p);
      if (!label.empty())
        ss << " (" << label << ")";
    }
  }

  LOG(llvl::info) << ss.rdbuf();
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

void fancon::test(Controller &sc, uint testRetries, bool singleThread) {
  sc.writeConf(conf_path);

  auto fanUIDs = sc.getFans();
  if (fanUIDs.empty())
    LOG(llvl::warning) << "No fans were detected, try running 'sudo sensors-detect' first";

  if (!exists(Util::fancon_dir))
    create_directory(Util::fancon_dir);

  LOG(llvl::info) << "Starting tests. This may take some time (to ensure accurate results)";
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
  auto fan(fancon::Controller::getFan(uid));

  FanTestResult res;
  stringstream ss;
  for (; retries > 0; --retries) {
    res = fan->test();

    if (res.testable() && res.valid()) {
      FanInterface::writeTestResult(uid, res, uid.type);

      ss << uid << " passed\n";
      break;
    } else if (uid.type == DeviceType::FAN_NV)
      LOG(llvl::error)
        << "NVIDIA manual fan control CoolBit is not set."
        << "Please run 'sudo nvidia-xconfig --cool-bits=4', restart your X server (or reboot) and retry test";
  }

  if (retries <= 0)
    ss << uid << ((res.testable()) ? " results are invalid, consider running with more --retries (default is 4)"
                                   : " cannot be tested, driver unresponsive") << '\n';
  LOG(llvl::info) << ss.rdbuf();
}

void fancon::handleSignal(int sig) {
  switch (sig) {
  case SIGTERM:
  case SIGINT:
  case SIGABRT: fancon::daemon_state = DaemonState::STOP;
    break;
  case SIGHUP: fancon::daemon_state = DaemonState::RELOAD;
    break;
  default: LOG(llvl::warning) << "Unknown signal caught: " << strsignal(sig);
  }
}

void fancon::start(Controller &sc, const bool fork_) {
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

    // refresh pid file to reflect forks
    write<pid_t>(Util::pid_file, getpid());
  }

  // Set file mode
  umask(0);

  // Change working directory
  if (chdir("/") < 0)
    LOG(llvl::warning) << "Failed to set working directory";

  auto daemon = sc.loadDaemon(conf_path, fancon::daemon_state);
  if (daemon->fans.empty()) {
    LOG(llvl::info) << "No fan configurations found, exiting fancond. See 'fancon -help'";
    return;
  }

  vector<thread> threads;
  auto sensorThreadTasks = getTasks(sc.conf.threads, daemon->sensors);
  for (auto &itPair : sensorThreadTasks)
    threads.emplace_back(thread([&daemon, &itPair] { daemon->readSensors(itPair.first, itPair.second); }));

  // Wait some time so the threads don't run together
  sleep_for(chrono::duration_cast<chrono::milliseconds>(sc.conf.update_time) / 2);

  auto fanThreadTasks = getTasks(sc.conf.threads, daemon->fans);
  for (auto &itPair : fanThreadTasks)
    threads.emplace_back(thread([&daemon, &itPair] { daemon->updateFans(itPair.first, itPair.second); }));

  // handle system signals
  struct sigaction act;
  act.sa_handler = fancon::handleSignal;
  sigemptyset(&act.sa_mask);
  for (auto &s : {SIGTERM, SIGINT, SIGABRT, SIGHUP})
    sigaction(s, &act, nullptr);

  LOG(llvl::debug) << "fancond started with " << threads.size() << " threads";
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
    const string message =
        "First use: run 'sudo fancon test && sudo fancon -lf -ls', then configure fan profiles in /etc/fancon.conf";
    LOG(llvl::info)
      << bold << setw(static_cast<int>(message.size())) << std::setfill('-') << left << '-' << '\n'
      << red << message << resetF << '\n'
      << bold << setw(static_cast<int>(message.size())) << left << '-' << resetF << "\n\n";
  }

  vector<string> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(string(argv[i]));

  fancon::Command help("help", false, false), start("start"), stop("stop"), reload("reload"),
      list_fans("list-fans", true), list_sensors("list-sensors", true),
      test("test"), write_config("write-config", true);
  vector<reference_wrapper<fancon::Command>> commands
      {help, start, stop, reload, list_fans, list_sensors, test, write_config};

  fancon::Option verbose("verbose"), quiet("quiet"), threads("threads", true), fork("fork"), retries("retries", true);
  fancon::Option debug("debug");    // DEPRECATED - use verbose; warnings will be displayed upon use
  vector<reference_wrapper<fancon::Option>> options
      {verbose, debug, quiet, threads, fork, retries};

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
        LOG(llvl::error) << "Please run with sudo, or as root for command: " << o.name << '\n';
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
          LOG(llvl::error) << "Option '" << *a << "' requires a value (>= 0)\n";

      } else
        o.called = true;

      continue;
    }

    LOG(llvl::warning) << "Unknown argument: " << *a << '\n';
  }

  // set log level
  if (debug.called) {   // DEPRECATED - TODO: remove after 07/2017
    verbose.called = true;
    LOG(llvl::warning) << "'-debug' option is deprecated, and WILL BE REMOVED. Use '-verbose'";
  }

  if (verbose.called)
    fancon::log::setLevel(llvl::debug);
  else if (quiet.called)
    fancon::log::setLevel(llvl::error);

  // Execute called commands with called options
  // Return after calling options before use of Controller to avoid unnecessary initialization
  if (help.called || args.empty()) { // execute help() if no commands are given
    fancon::help();
    return 0;
  }
  if (stop.called) {
    fancon::send(DaemonState::STOP);
    return 0;
  }
  if (reload.called) {
    fancon::send(DaemonState::RELOAD);
    return 0;
  }

  Controller sc(threads.val);

  if (list_fans.called)
    fancon::listFans(sc);
  if (list_sensors.called)
    fancon::listSensors(sc);
  if (write_config.called || !exists(conf_path))
    sc.writeConf(conf_path);

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
    LOG(llvl::warning) << "A fancon process is already running\n";

  return 0;
}