#include "main.hpp"

namespace f = fancon;

void f::help() {
  using fancon::serialization_constants::controller_config::threads_prefix;

  LOG(llvl::info)
    << "Usage:\n"
    << "  fancon <command> [options]\n\n"
    << "list-fans     -lf  Lists the UIDs of all fans\n"
    << "list-sensors  -ls  List the UIDs of all sensors\n"
    << "write-config  -wc  Appends missing fan UIDs to " << config_path << '\n'
    << "test               Tests the characteristic of all fans - REQUIRED for RPM / percentage configuration\n"
    << "  -r retries       Retry attempts for a failing test - increase for failing tests (default: 4)\n"
    << "start              Starts the fancon daemon\n"
    << "  -f fork          Forks off the parent process\n"
    << "  -t threads       Override \"" << threads_prefix << "\" in " << config_path << '\n'
    << "stop               Stops the fancon daemon\n"
    << "reload        -re  Reloads the fancon daemon\n\n"
    << "Global options:\n"
    << "  -v verbose       Output all messages\n"
    << "  -q quiet         Output only error or fatal messages\n\n"
    << "Logging: Logs are stored at " << fancon::log::log_path << " or in the systemd journal (if applicable)\n"
    << "NVIDIA Support: Install the following packages (libraries): "
    << "libx11-6 (libX11.so); libxnvctrl0 (libXNVCtrl.so)";
}

void f::suggestUsage(const char *fanconDir, const char *configPath) {
  // Constants to modify console text font
  const char *bold = "\033[1m", *red = "\033[31m", *resetFont = "\033[0m";

  const bool suggestTest = !exists(fanconDir),
      suggestConfig = !exists(configPath);

  if (suggestTest || suggestConfig) {
    std::ostringstream messages;
    int maxSize{};

    if (suggestTest) {
      constexpr const char *m =
          "Run `sudo fancon test` to be able to configure fan profiles using RPM, or RPM percentage";
      constexpr const auto mSize = static_cast<int>(strlength(m));
//      constexpr const auto mSize = static_cast<int>(std::char_traits<char>::length(m)); // TODO: C++17
      if (mSize > maxSize)
        maxSize = mSize;

      messages << bold << red << m << '\n';
    }

    if (suggestConfig) {
      const string m =
          string("Edit ") + configPath + " to configure fan profiles, referencing `sudo fancon list-sensors`";
      const auto mSize = static_cast<int>(m.size());
      if (mSize > maxSize)
        maxSize = mSize;

      messages << resetFont << red << m << '\n';
    }

    LOG(llvl::info) << bold << setw(maxSize) << std::setfill('-') << left << '-' << '\n'
                    << resetFont << messages.str() << resetFont
                    << bold << setw(maxSize) << std::setfill('-') << left << '-' << '\n' << resetFont;
  }
}

void f::listFans() {
  auto uids = Devices::getFanUIDs();
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
      sst << read<rpm_t>(p.rpm_min_pf, hwID, uid.type) << " - " << read<rpm_t>(p.rpm_max_pf, hwID, uid.type);
      ss << setw(scw) << left << sst.str() << read<pwm_t>(p.pwm_min_pf, hwID, uid.type) << " (or 0)";
    } else
      untested = true;

    ss << '\n';
  }

  if (untested)
    ss << "\nNote: un-tested fans have no values";

  LOG(llvl::info) << ss.rdbuf();
}

void f::listSensors() {
  auto uids = Devices::getSensorUIDs();
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

void f::appendConfig(const string &path) {
  umask(default_umask);
  std::ifstream ifs(path);

  auto allUIDs = Devices::getFanUIDs();
  vector<decltype(allUIDs)::iterator> existingUIDs;

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
    os << "# Interval is the seconds between fan speed changes\n"
       << "# Dynamic enables interpolation between points, e.g. 30:20% 40:30%, @ 35°C, RPM is 25%\n"
       << controller::Config() << "\n\n"
       << "# Missing <Fan UID>'s are appended with 'fancon write-config' or manually with 'fancon list-fans'\n"
       << "# <Sensor UID>s can be enumerated with 'fancon list-sensors'\n"
       << "# <Fan Config>s are comprised of one or more points\n"
       << "#\n"
       << "# Note. 'fancon test' MUST be run for RPM & percentage speed control\n"
       << "# Point syntax: Temperature(f):RPM(%);PWM\n"
       << "#               REQUIRED: Temperature, and RPM or PWM\n"
       << "#               OPTIONAL: 'f' for temperature in fahrenheit\n"
       << "#                         '%' for percentage of max RPM control - 1% being the slowest spinning speed\n"
       << "#\n"
       << "# Example:\n"
       << "# it8728/2:fan1 coretemp/0:temp2   0:0% 25:10% 113f:30% 55:50% 65:1000 80:1400 190:100%\n"
       << "# 0:0%     (or 32f;0)  -> fan stopped below 25°C\n"
       << "# 25:10%   (or 77f:5%) -> fan starts -- 10% of max speed @ 25°C (77 fahrenheit)\n"
       << "# 113f:30% (or 45:10%) -> 30% @ 30°C (113 fahrenheit)\n"
       << "# 65:1000  (or 65;180) -> 1000 RPM @ 65°C -- where 1000 RPM is reached at 180 PWM\n"
       << "# 90:100%  (or 90;255) -> 100% (max) @ 90°C\n"
       << "#\n"
       << "# <Fan UID>     <Sensor UID>     <Fan Config>\n";
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
  if (!ofs)
    LOG(llvl::error) << "Failed to write config: " << path;
}

void f::testFans(uint testRetries, bool singleThreaded) {
  umask(default_umask);
  appendConfig(config_path);

  auto fanUIDs = Devices::getFanUIDs();
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

    threads.emplace_back(thread(&f::testFan, std::ref(*it), Devices::getFan(*it), testRetries));

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

    // Redirect standard file descriptors to /dev/null
    stdin = fopen("/dev/null", "r");
    stdout = fopen("/dev/null", "r+");
    stderr = fopen("/dev/null", "r+");

    // Create a new session for the child
    if ((pid = setsid()) < 0) {
      LOG(llvl::fatal) << "Failed to fork off parent";
      exit(EXIT_FAILURE);
    }

    // fork again
    pid = fork();
    exitParent(pid);

    // refresh pid file to reflect forks
    write<pid_t>(Util::pid_path, getpid());
  }

  umask(f::default_umask);

  // Change working directory
  if (chdir("/") < 0)
    LOG(llvl::warning) << "Failed to set working directory";

  Controller controller(config_path);
  while (controller.run() == ControllerState::reload) {
    controller.reload(config_path);
    LOG(llvl::info) << "Reloading...";
  }

  return;
}

void f::sendSignal(ControllerState state) {
  // use kill() to send signal to the process
  if (Util::locked())
    kill(read<pid_t>(Util::pid_path), static_cast<int>(state));
  else if (state == ControllerState::reload)
    LOG(llvl::info) << "Cannot reload: fancond is not running";
}

bool f::Option::setIfValid(const string &str) {
  istringstream ss(str);  // TODO: C++17 - use from_chars
  ss >> val;

  return (called = !ss.fail());
}

int main(int argc, char *argv[]) {
#ifdef FANCON_PROFILE
  ProfilerStart("fancon_profiler");
  HeapProfilerStart("fancon_heap_profiler");
#endif //FANCON_PROFILE

  vector<string> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(string(argv[i]));

  // Options and commands
  f::Option verbose("verbose", "v"), quiet("quiet", "q"),
      threads("threads", "t", true), fork("fork", "f"), retries("retries", "r", true),
      debug("debug", "d");    /// <\deprecated Use verbose  TODO: remove 07/2017
  vector<reference_wrapper<f::Option>> options
      {verbose, debug, quiet, threads, fork, retries};

  f::Command help("help", "", &f::help, false, false, args.empty()),
      list_fans("list-fans", "lf", &f::listFans),
      list_sensors("list-sensors", "ls", &f::listSensors),
      write_config("write-config", "wc", [] { f::appendConfig(config_path); }),
      test("test", "", [&retries, &threads] {
    f::testFans((retries.called && retries.val > 0) ? retries.val : 4,
                (threads.called && threads.val == 1));
  }, true),
      start("start", "", [&fork] { f::start(fork.called); }, true),
      stop("stop", "", [] { f::sendSignal(ControllerState::stop); }),
      reload("reload", "re", [] { f::sendSignal(ControllerState::reload); });
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
    auto cIt_tc = find_if(commands.begin(), commands.end(), [&](auto &c) { return c.get() == *a; });
    if (cIt_tc != commands.end()) {
      cIt_tc->get().called = true;
      continue;
    }

    // Search options
    auto oIt = find_if(options.begin(), options.end(), [&](auto &o) { return o.get() == *a; });
    if (oIt != options.end()) {
      auto &o = oIt->get();

      // Handle value if required
      if (o.has_value) {
        // Validate the next argument, and store value
        auto na = next(a);

        if (na != args.end() && o.setIfValid(*na))
          ++a;  // Skip arg after successful use
        else
          LOG(llvl::error) << "Option '" << *a << "' requires a value (>= 0)\n";

      } else
        o.called = true;

      continue;
    }

    LOG(llvl::warning) << "Unknown argument: " << *a << '\n';
  }

  // Set log level
  if (debug.called) {
    verbose.called = true;
    LOG(llvl::warning) << "'-debug' option is deprecated, and WILL BE REMOVED. Use '-verbose'";
  }

  llvl logLevel = llvl::info; // Default
  if (verbose.called)
    logLevel = llvl::debug;
  else if (quiet.called)
    logLevel = llvl::error;
  fancon::log::setLevel(logLevel);

  // Suggest usage
  f::suggestUsage(Util::fancon_dir, config_path);

  // Run called commands
  for (const auto &c : commands) {
    if (c.get().called) {
      const auto &com = c.get();

      // Command requirements must be met before running
      if (com.lock && !Util::try_lock()) {
        LOG(llvl::warning) << "A fancon process is already running\n";
        exit(EXIT_FAILURE); // Exiting failing so init system does not restart
      } else if (com.require_root && getuid() != 0)
        LOG(llvl::error) << "Please run with sudo, or as root for command: " << com.name << '\n';
      else
        com.func();

      break;
    }
  }

#ifdef FANCON_PROFILE
  HeapProfilerStop();
  ProfilerStop();
#endif //FANCON_PROFILE

  return 0;
}