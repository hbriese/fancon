#include "main.hpp"

namespace f = fancon;

void f::help() {
  LOG(llvl::info)
    << "Usage:\n"
    << "  fancon <command> [options]\n\n"
    << "Available commands (and options <default>):\n"
    << "-lf list-fans      Lists the UIDs of all fans\n"
    << "-ls list-sensors   List the UIDs of all temperature sensors\n"
    << "-wc write-config   Appends missing fan UIDs to " << conf_path << '\n'
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
    << "Logging: Logs are stored at " << fancon::log::log_path << " or in the systemd journal (if applicable)\n"
    << "NVIDIA Support: Install the following packages (libraries): "
    << "libx11-6 (libX11.so); libxnvctrl0 (libXNVCtrl.so)";
}

void f::listFans() {
  auto uids = Find::getFanUIDs();
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

    FanInterfacePaths p(uid);
    string hwID = to_string(uid.hw_id);
    if (p.tested()) {
      sst.str("");
      // Read stored data about device without instantiating it
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

void f::listSensors() {
  auto uids = Find::getSensorUIDs();
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

void f::testFans(uint testRetries, bool singleThreaded) {
  appendConfig(conf_path);

  auto fanUIDs = Find::getFanUIDs();
  if (fanUIDs.empty()) {
    LOG(llvl::warning) << "No fans were detected, try running 'sudo sensors-detect' first";
    return;
  }

  if (!exists(Util::fancon_dir))
    create_directory(Util::fancon_dir);

  LOG(llvl::info) << "Starting tests. This may take some time (to ensure accurate results)";
  vector<thread> threads;
  for (auto it = fanUIDs.begin(); it != fanUIDs.end(); ++it) {
    auto dir = Util::getDir(to_string(it->hw_id), it->type);
    if (!exists(dir))
      create_directory(dir);

    threads.emplace_back(thread(&f::testFan, std::ref(*it), Find::getFan(*it), testRetries));

    if (singleThreaded) {
      threads.back().join();
      threads.pop_back();
    }
  }

  // Rejoin threads
  for (auto &t : threads)
    if (t.joinable())
      t.join();
    else
      t.detach();
}

void f::testFan(const UID &uid, unique_ptr<FanInterface> &&fan, uint retries) {
  FanTestResult res;
  stringstream ss;
  for (; retries > 0; --retries) {
    res = fan->test();

    if (res.testable() && res.valid()) {
      FanInterface::writeTestResult(uid, res, uid.type);
      ss << uid << " passed";
      break;
    } else if (uid.type == DeviceType::fan_nv)
      LOG(llvl::error)
        << "NVIDIA manual fan control coolbit is not set. "
        << "Please run 'sudo nvidia-xconfig --cool-bits=4', restart your X server (or reboot) and retry test";
  }

  if (retries <= 0)
    ss << uid << ((res.testable()) ? " results are invalid, consider running with more --retries (default is 4)"
                                   : " cannot be tested, driver unresponsive");
  LOG(llvl::info) << ss.rdbuf();
}

/// \bug ifstream failbit, or badbit is set during read, consequently reporting a fail to the user
void f::appendConfig(const string &path) {
  std::ifstream ifs(path);

  auto allUIDs = Find::getFanUIDs();
  vector<decltype(allUIDs.begin())> existingUIDs;

  bool pExists = exists(path);
  bool controllerConfigFound = false;

  // Read fan UIDs from config
  if (pExists) {
    LOG(llvl::info) << "Config exists, adding absent fans";

    for (string line; std::getline(ifs, line) && !ifs.eof();) {
      if (!Controller::validConfigLine(line))
        continue;

      istringstream liss(line);
      if (!controllerConfigFound) {
        if (controller::Config(liss).valid()) {
          controllerConfigFound = true;
          continue;
        } else
          liss.seekg(0, std::ios::beg);
      }

      // Add UID to list
      UID fanUID(liss);
      auto it = find(allUIDs.begin(), allUIDs.end(), fanUID);
      if (fanUID.valid(DeviceType::fan_interface) && it != allUIDs.end())
        existingUIDs.emplace_back(move(it));
    }
  }

  // Append missing info to the config
  ofstream ofs(path, std::ios_base::app);

  auto writeTop = [](ostream &os) {
    os << "# Update is the seconds between speed changes\n"
       << "# Dynamic interpolates between points, e.g. [30:20%] [40:30%], @ 35°C, RPM is 25%\n"
       << controller::Config() << "\n\n"
       << "# Missing <Fan UID>'s are appended with 'fancon write-config' or manually with 'fancon list-fans'\n"
       << "# <Sensor UID>'s can be enumerated with 'fancon list-sensors'\n"
       << "# <Fan Config>'s are comprised of one or more points\n"
       << "# Point syntax: [TEMP (f) :RPM (%) ;PWM] - 'f' & '%' are optional"
       << '\n'
       << "# Example:\n"
       << "# it8728/2:fan1 coretemp/0:temp2   [0:0%] [86f:10%] [45:30%] [55:1000] [65:1200] [90:100%]\n"
       << "# [0:0%]    (or [0;0])    -> fan stopped below 30°C\n"
       << "# [86f:10%] (or [30:10%]) -> 10% of max fan speed @ 30°C (86 fahrenheit)\n"
       << "# [65:1200] (or [65;180]) -> 1200 RPM @ 65°C -- where 1200 RPM is reached at 180 PWM\n"
       << "# [90:100%] (or [90;255]) -> max fan speed @ 90°C\n\n"
       << "# 'fancon test' MUST be run before use of RPM for speed control. PWM control can still be used without\n"
       << "# Append 'f' for temperature in fahrenheit e.g. [86f:10%]\n"
       << "#        '%' for percentage of max RPM control - 1% being the slowest speed whilst still spinning\n\n"
       << "# <Fan UID>     <Sensor UID>    <[temperature :speed (RPM) ;PWM (0-255)]>\n";
  };

  if (!pExists) {
    LOG(llvl::info) << "Writing new config: " << path;
    writeTop(ofs);
  }

  // Append UIDs not found in config (existingUIDs)
  for (auto it = allUIDs.begin(); it != allUIDs.end(); ++it)
    if (find(existingUIDs.begin(), existingUIDs.end(), it) == existingUIDs.end())    // Not currently configured
      ofs << *it << '\n';

  ofs.close();
  if (ofs.fail())
    LOG(llvl::error) << "Failed to write config: " << path;
}

void f::start(const bool fork_) {
  if (fork_) {
    LOG(llvl::info) << "Starting fancond forked; see 'fancon -help' for logging info";

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

  Controller controller(conf_path);
  while (controller.run() == ControllerState::reload)
    controller.reload(conf_path);

  return;
}

void f::sendSignal(ControllerState state) {
  // use kill() to send signal to the process
  if (Util::locked())
    kill(read<pid_t>(Util::pid_file), static_cast<int>(state));
  else if (state == ControllerState::reload)
    LOG(llvl::info) << "Cannot reload: fancond is not running";
}

int main(int argc, char *argv[]) {
#ifdef FANCON_PROFILE
  ProfilerStart("fancon_full");
  HeapProfilerStart("fancon_full");
#endif //FANCON_PROFILE

  vector<string> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(string(argv[i]));

  if (!exists(Util::fancon_dir) || !exists(conf_path)) {
    create_directory(Util::fancon_dir);
    const char *bold = "\033[1m", *red = "\033[31m", *resetFont = "\033[0m";
    const string message =
        "First use: run 'sudo fancon test && sudo fancon -lf -ls', then configure fan profiles in /etc/fancon.conf";
    LOG(llvl::info)
      << bold << setw(static_cast<int>(message.size())) << std::setfill('-') << left << '-' << '\n'
      << red << message << resetFont << '\n'
      << bold << setw(static_cast<int>(message.size())) << left << '-' << resetFont << "\n\n";

    // TODO: Offer to run `test`
  }

  // Options and commands
  f::Option verbose("verbose", "v"), quiet("quiet", "q"),
      threads("threads", "t", true), fork("fork", "f"), retries("retries", "r", true);
  f::Option debug("debug", "d");    /// <\deprecated Use verbose
  vector<reference_wrapper<f::Option>> options
      {verbose, debug, quiet, threads, fork, retries};

  f::Command help("help", "", &f::help, false, false, args.empty()),
      list_fans("list-fans", "lf", &f::listFans),
      list_sensors("list-sensors", "ls", &f::listSensors),
      write_config("write-config", "wc", [] { f::appendConfig(conf_path); }),
      test("test", "", [&retries, &threads] {
    f::testFans((retries.called && retries.val > 0) ? retries.val : 4,
                (threads.called && threads.val == 1));
  }, true),
      start("start", "", [&fork] { f::start(fork.called); }, true),
      stop("stop", "", [] { f::sendSignal(ControllerState::stop); }),
      reload("reload", "", [] { f::sendSignal(ControllerState::reload); });
  vector<reference_wrapper<f::Command>> commands = {
      help, list_fans, list_fans, list_sensors, write_config, test, start, stop, reload
  };
  // TODO: static_assert(constexpr !duplicates, "Command names, and or short names cannot be the same");

  for (auto a = args.begin(), end = args.end(); a != end; ++a) {
    if (a->empty())
      continue;

    // Remove all preceding '-'
    a->erase(a->begin(), find_if(a->begin(), a->end(), [](const char &c) { return c != '-'; }));

    // Convert to lower case
    std::transform(a->begin(), a->end(), a->begin(), ::tolower);

    // Search commands
    auto cIt_tc = find_if(commands.begin(), commands.end(), [&a](auto &c) { return c.get() == *a; });
    if (cIt_tc != commands.end()) {
      cIt_tc->get().called = true;
      continue;
    }

    // Search options
    auto oIt = find_if(options.begin(), options.end(), [&a](auto &o) { return o.get() == *a; });
    if (oIt != options.end()) {
      auto &o = oIt->get();

      // Handle value if required
      if (o.has_value) {
        // Validate the next argument, and store value
        auto na = next(a);

        if (na != args.end() && Util::isNum(*na)) {
          o.val = static_cast<uint>(std::stoul(*na));
          o.called = true;
          ++a;  // Skip next arg (na) in current args loop
        } else
          LOG(llvl::error) << "Option '" << *a << "' requires a value (>= 0)\n";

      } else
        o.called = true;

      continue;
    }

    LOG(llvl::warning) << "Unknown argument: " << *a << '\n';
  }

  // Set log level
  if (debug.called) {   // DEPRECATED - TODO: remove 07/2017
    verbose.called = true;
    LOG(llvl::warning) << "'-debug' option is deprecated, and WILL BE REMOVED. Use '-verbose'";
  }

  llvl logLevel = llvl::info; // Default
  if (verbose.called)
    logLevel = llvl::debug;
  else if (quiet.called)
    logLevel = llvl::error;
  fancon::log::setLevel(logLevel);

  // Run called commands
  for (const auto &c : commands) {
    if (c.get().called) {
      auto &com = c.get();

      // Command requirements must be met before running
      if (com.lock && !Util::try_lock())
        LOG(llvl::warning) << "A fancon process is already running\n";
      else if (com.require_root && getuid() != 0)
        LOG(llvl::error) << "Please run with sudo, or as root for command: " << com.name << '\n';
      else
        com.func();

      break;
    }
  }

#ifdef HEAP_PROFILE
  ProfilerStop();
  HeapProfilerDump("End of main");
  HeapProfilerStop();
#endif //HEAP_PROFILE

  return 0;
}