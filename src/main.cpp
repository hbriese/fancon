#include "main.hpp"

namespace f = fancon;
namespace Util = fancon::Util;

void f::help(const char *configPath) {
  using fancon::serialization_constants::controller_config::threads_prefix;

  LOG(llvl::info)
    << "Usage:\n"
    << "  fancon <command> [options]\n\n"
    << "list-fans     -lf  Lists the UIDs of all fans\n"
    << "list-sensors  -ls  List the UIDs of all sensors\n"
    << "write-config  -wc  Appends missing fan UIDs to " << configPath << '\n'
    << "test               Tests the characteristic of all fans - REQUIRED "
        "for RPM / percentage configuration\n"
    << "  -c config        Config path\n"
    << "  -r retries       Retry attempts for a failing test - increase for "
        "failing tests (default: 4)\n"
    << "start         -s   Starts the fancon daemon\n"
    << "  -c config        Config path\n"
    << "  -f fork          Forks off the parent process\n"
    << "  -t threads       Overrides \"" << threads_prefix
    << "\" in " << configPath << '\n'
    << "stop          -S   Stops the fancon daemon\n"
    << "reload        -re  Reloads the fancon daemon\n\n"
    << "Global options:\n"
    << "  -v verbose       Output all messages\n"
    << "  -q quiet         Output only error or fatal messages\n\n"
    << "Logging: Logs are stored at " << fancon::log::log_path
    << " or in the systemd journal (if applicable)\n"
    << "NVIDIA support requires: "
    << "libx11-6 (libX11.so), and libxnvctrl0 (libXNVCtrl.so)";
}

void f::suggestUsage(const char *fanconDir, const char *configPath) {
  // Constants to modify console text font
  const char *bold = "\033[1m", *red = "\033[31m", *resetFont = "\033[0m";

  const bool suggestTest = !exists(fanconDir),
      suggestConfig = !exists(configPath);

  if (suggestTest || suggestConfig) {
    std::ostringstream messages;
    int textLen{};

    if (suggestTest) {
      constexpr const char *m = "Run 'sudo fancon test' to enable fan "
          "configuration using RPM, or RPM percentage - "
          "current only PWM control is enabled";
      constexpr const auto mSize = static_cast<int>(strlength(m));
      // constexpr const auto mSize =
      // static_cast<int>(std::char_traits<char>::length(m)); // TODO C++17
      if (mSize > textLen)
        textLen = mSize;

      messages << bold << red << m << '\n';
    }

    if (suggestConfig) {
      const string m = string("Edit ") + configPath + " to configure fan "
          "profiles, referencing 'sudo fancon list-sensors'";
      const auto mSize = static_cast<int>(m.size());
      if (mSize > textLen)
        textLen = mSize;

      messages << resetFont << red << m << '\n';
    }

    // Use the lesser of terminal or text width
    const auto termLen = f::getTerminalWidth();
    const auto &width = (termLen > 0 && termLen < textLen) ? termLen : textLen;

    LOG(llvl::info)
      << bold << setw(width) << std::setfill('-') << left << '-' << '\n'
      << resetFont << messages.str() << resetFont << bold
      << setw(width) << std::setfill('-') << left << '-' << resetFont;
  }
}

unsigned short f::getTerminalWidth() {
  struct winsize w;
  ioctl(STDIN_FILENO, TIOCGWINSZ, &w);

  return w.ws_col;
}

void f::listFans() {
  auto uids = Devices::getFanUIDs();

  if (uids.empty()) {
    LOG(llvl::info)
      << "No fans were detected, try running 'sudo sensors-detect' first\n";
    return;
  }

  vector<pair<UID, string>> uidPairs;
  uidPairs.reserve(uids.size());

  // Determine max column width, and get UID text
  int uidWidth{1};
  for (auto &u : uids) {
    stringstream fss;
    fss << u;

    if (fss.tellp() > uidWidth)
      uidWidth = static_cast<int>(fss.tellp()) + 1;

    uidPairs.emplace_back(std::make_pair(move(u), fss.str()));
  }

  constexpr const auto rpmColWidth = 4 + 1 + 4 + 1;  // Space for 'XXXX-XXXX '
  stringstream headerSS;
  headerSS << setw(uidWidth) << left << "<UID>"
           << setw(rpmColWidth) << left << "<RPM>"
           << "<PWM>\n";

  stringstream dataSS;
  bool untested = false;
  for (const auto &up : uidPairs) {
    dataSS << setw(uidWidth) << left << up.second;

    FanCharacteristicPaths p(up.first);
    string hwID = to_string(up.first.hw_id);
    if (p.tested()) {
      // Read stored data about device without instantiating it
      const string rpmData =
          to_string(read<rpm_t>(p.rpm_min_pf, hwID, up.first.type)) + '-'
              + to_string(read<rpm_t>(p.rpm_max_pf, hwID, up.first.type));

      const string pwmData =
          to_string(read<pwm_t>(p.pwm_min_pf, hwID, up.first.type)) + '-'
              + to_string(read<pwm_t>(p.pwm_max_pf, hwID, up.first.type));

      dataSS << setw(rpmColWidth) << left << rpmData << pwmData;
    } else
      untested = true;

    dataSS << '\n';
  }

  if (untested)
    dataSS << "\nNote: values are missing for un-tested fans";

  LOG(llvl::info) << headerSS.rdbuf() << dataSS.rdbuf();
}

void f::listSensors() {
  auto uids = Devices::getSensorUIDs();
  stringstream ss;

  ss << ((!uids.empty()) ? "<Sensor UID>" : "No sensors were detected, try "
      "running `sudo sensors-detect` first");

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
  vector<decltype(allUIDs)::iterator> oldUIDs;

  bool pExists = exists(path);
  bool controllerConfigFound = false;

  // Read fan UIDs from config
  if (pExists) {

    for (string line; std::getline(ifs, line) && !ifs.eof();) {
      if (!Controller::validConfigLine(line))
        continue;

      istringstream liss(line);
      if (!controllerConfigFound) {
        if (controller::Config(liss).valid()) {
          controllerConfigFound = true;
          continue;
        }

        liss.seekg(0, std::ios::beg);
      }

      // Add UID to list
      UID fanUID(liss);
      auto it = find(allUIDs.begin(), allUIDs.end(), fanUID);
      if (fanUID.valid(DeviceType::fan_interface) && it != allUIDs.end())
        oldUIDs.emplace_back(move(it));
    }
  }

  // Append missing info to the config
  ofstream ofs(path, std::ios_base::app);

  auto writeHeader = [](ostream &os) {
    os << "# Profile is the active profile to be used"
       << "# Interval is the seconds between fan speed changes\n"
       << "# Dynamic enables interpolation between points, e.g. 30:20% 40:30%, "
           "@ 35°C, RPM is 25%\n"
       << controller::Config() << "\n\n"
       << "# Missing <Fan UID>'s are appended with 'fancon write-config' "
           "or manually with 'fancon list-fans'\n"
       << "# <Sensor UID>s can be enumerated with 'fancon list-sensors'\n"
       << "# <Fan Config>s are comprised of one or more points\n"
       << "#\n"
       << "# Note. 'fancon test' MUST be run for RPM & percentage speed "
           "control\n"
       << "# Point syntax: Temperature(f):RPM(%);PWM\n"
       << "#               REQUIRED: Temperature, and RPM or PWM\n"
       << "#               OPTIONAL: 'f' for temperature in fahrenheit\n"
       << "#                         '%' for percentage of max RPM control - "
           "1% being the slowest spinning speed\n"
       << "#\n"
       << "# Example:\n"
       << "# it8728/2:fan1 coretemp/0:temp2   0:0% 25:10% 113f:30% 55:50% "
           "65:1000 80:1400 190:100%\n"
       << "# 0:0%     (or 32f;0)  -> fan stopped below 25°C\n"
       << "# 25:10%   (or 77f:5%) -> fan starts -- 10% of max speed @ 25°C "
           "(77 fahrenheit)\n"
       << "# 113f:30% (or 45:10%) -> 30% @ 30°C (113 fahrenheit)\n"
       << "# 65:1000  (or 65;180) -> 1000 RPM @ 65°C -- where 1000 RPM is "
           "reached at 180 PWM\n"
       << "# 90:100%  (or 90;255) -> 100% (max) @ 90°C\n"
       << "#\n"
       << "# Profiles can be specified with '>' followed by a string\n"
       << "> default\n"
       << "# <Fan UID>     <Sensor UID>     <Fan Config>\n";
  };

  // Append missing UIDs and header
  size_t newFans = 0;
  if (!pExists) {
    LOG(llvl::info) << "Writing new config to " << path;
    writeHeader(ofs);

    newFans = allUIDs.size();
    for (auto &it : allUIDs)
      ofs << it << '\n';

  } else {
    for (auto it = allUIDs.begin(), end = allUIDs.end(); it != end; ++it)
      if (find(oldUIDs.begin(), oldUIDs.end(), it) == oldUIDs.end()) {
        ofs << *it << '\n';
        ++newFans;
      }
  }

  if (newFans)
    LOG(llvl::info) << "Adding " << newFans << " new fans";

  ofs.close();
  if (!ofs)
    LOG(llvl::error) << "Failed to write config: " << path;
}

void f::testFans(uint testRetries,
                 const char *configPath,
                 bool singleThreaded) {
  umask(default_umask);
  appendConfig(configPath);

  auto fanUIDs = Devices::getFanUIDs();
  if (fanUIDs.empty()) {
    LOG(llvl::warning)
      << "No fans were detected, try running 'sudo sensors-detect' first";
    return;
  }

  if (!exists(Util::fancon_dir))
    create_directory(Util::fancon_dir);

  LOG(llvl::info)
    << "Starting tests. This may take some time (to ensure accurate results)";
  vector<thread> threads;
  for (const auto &uid : fanUIDs) {
    auto dir = Util::getDir(to_string(uid.hw_id), uid.type);
    if (!exists(dir))
      create_directory(dir);

    threads.emplace_back(
        thread(&f::testFan, std::ref(uid), Devices::getFan(uid), testRetries));

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
  stringstream ss;
  FailState fstate;

  auto availRetries = retries;
  for (; availRetries > 0; --availRetries) {
//    auto [res, failReason] = fan->test(); // TODO C++17
    FanCharacteristics c;
    std::tie(c, fstate) = fan->test();

    if (c.valid() && fstate == FailState::null) {
      FanInterface::writeTestResult(uid, c, uid.type);
      break;
    }

    if (uid.type == DeviceType::fan_nv) {
      LOG(llvl::error) << "NVIDIA manual fan control coolbit is not set. "
                       << "Please run 'sudo nvidia-xconfig --cool-bits=4', "
                           "restart your X server (or reboot) and retry test";
      break;
    }
  }

  // Write fail, or success message
  ss << uid;
  if (availRetries <= 0) {
    ss << " failed: ";

    if (fstate != FailState::null) {
      switch (fstate) {
      case FailState::control:
        ss << "unable to acquire control (potential driver issue)";
        break;
      case FailState::pwm_write: ss << "cannot write PWM to device";
        break;
      case FailState::rpm_read_accuracy:
        ss << "unresponsive fan (likely nothing plugged in)";
        break;
      default:static_assert(true, "Unhandled test fail state");
      }
    } else
      ss << " results are invalid, consider running with "
          "'--retries' greater than " + to_string(retries);
  } else
    ss << " passed";

  LOG(llvl::info) << ss.rdbuf();
}

void f::forkOffParent() {
  auto pid = fork();

  if (pid > 0)
    exit(EXIT_SUCCESS);
  else if (pid < 0) {
    LOG(llvl::fatal) << "Failed to fork off parent";
    exit(EXIT_FAILURE);
  }
}

void f::start(const char *configPath, const bool fork_) {
  umask(f::default_umask);

  // Change working directory
  if (chdir("/") < 0)
    LOG(llvl::warning) << "Failed to set working directory";

  if (fork_) {
    LOG(llvl::info)
      << "Starting fancon daemon forked; see 'fancon -help' for logging info";

    // Daemononize process by forking, creating new session, then forking again
    f::forkOffParent();

    // Create a new session for the child
    if (setsid() < 0) {
      LOG(llvl::fatal) << "Failed to fork off parent";
      exit(EXIT_FAILURE);
    }

    f::forkOffParent();

    // Update PID file to reflect forks
    write(Util::pid_path, getpid());

    // Redirect standard file descriptors to /dev/null
    stdin = fopen("/dev/null", "r");
    stdout = fopen("/dev/null", "r+");
    stderr = fopen("/dev/null", "r+");
  }

  // Load config into controller & run, looping while signaled to reload
  while (Controller(configPath).run() == ControllerState::reload)
    LOG(llvl::info) << "Reloaded";
}

void f::signalState(ControllerState state) {
  // Use kill() to send signal to the process
  if (Util::locked())
    kill(read<pid_t>(Util::pid_path), static_cast<int>(state));
  else if (state == ControllerState::reload)
    LOG(llvl::info) << "Cannot reload: fancon daemon is not running";
}

bool f::Option::setIfValid(const string &str) {
  istringstream ss(str); // TODO C++17 - use from_chars
  ss >> val;

  return (called = !ss.fail());
}

int main(int argc, char *argv[]) {
#ifdef FANCON_PROFILE
  //  ProfilerStart("fancon_main");
    HeapProfilerStart("fancon_main");
#endif // FANCON_PROFILE

  // Store arguments, skipping preceding '-'
  vector<string> args;
  for (auto i = 1; i < argc; ++i) {
    auto *argp = argv[i];
    while (*argp == '-')
      ++argp;

    args.emplace_back(string(argp));
  }

  // Options and commands
  f::Option verbose("verbose", "v"), quiet("quiet", "q"),
      threads("threads", "t", true), fork("fork", "f"),
      retries("retries", "r", true),
      debug("debug", "d"); /// <\deprecated Use verbose  TODO remove 07/2017
  vector<reference_wrapper<f::Option>> options{verbose, debug, quiet,
                                               threads, fork, retries};

  // Config to use
  string configPath = Util::config_path_default;

  f::Command help("help", "", [&] { f::help(configPath.c_str()); },
                  false, false, args.empty()),
      list_fans("list-fans", "lf", &f::listFans),
      list_sensors("list-sensors", "ls", &f::listSensors),
      write_config("write-config", "wc", [&] { f::appendConfig(configPath); }),
      test("test", "",
           [&] {
             f::testFans(
                 (retries.called && retries.val > 0) ? retries.val : 4,
                 configPath.c_str(), (threads.called && threads.val == 1));
           }, true),
      start("start", "s",
            [&] { f::start(configPath.c_str(), fork.called); }, true),
      stop("stop", "S", [] { f::signalState(ControllerState::stop); }),
      reload("reload", "re", [] { f::signalState(ControllerState::reload); });
  vector<reference_wrapper<f::Command>> commands = {
      help, list_fans, list_fans, list_sensors,
      write_config, test, start, stop, reload};

  for (auto a = args.begin(), end = args.end(); a != end; ++a) {
    if (a->empty())
      continue;

    // Search commands
    auto cIt_tc = find_if(commands.begin(), commands.end(),
                          [&](auto &c) { return c.get() == *a; });
    if (cIt_tc != commands.end()) {
      cIt_tc->get().called = true;
      continue;
    }

    // Search options
    auto oIt = find_if(options.begin(), options.end(),
                       [&](auto &o) { return o.get() == *a; });
    if (oIt != options.end()) {
      auto &o = oIt->get();

      // Handle value if required
      if (o.has_value) {
        // Validate the next argument, and store value
        auto na = next(a);

        if (na != args.end() && o.setIfValid(*na))
          ++a; // Skip arg after successful use
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
    LOG(llvl::warning)
      << "'-debug' option is deprecated, and WILL BE REMOVED. Use '-verbose'";
  }

  llvl logLevel = llvl::info; // Default
  if (verbose.called)
    logLevel = llvl::debug;
  else if (quiet.called)
    logLevel = llvl::error;
  fancon::log::setLevel(logLevel);

  // Suggest usage
  f::suggestUsage(Util::fancon_dir, configPath.c_str());

  // Run called commands
  for (const auto &c : commands) {
    if (c.get().called) {
      const auto &com = c.get();

      // Command requirements must be met before running
      if (com.lock && !Util::try_lock()) {
        LOG(llvl::error) << "A fancon process is already running\n";
        return EXIT_FAILURE; // Exiting failing so init system does not restart
      }

      if (com.require_root && getuid() != 0) {
        LOG(llvl::error)
          << "Please run '" << com.name << "' with sudo, or as root\n";
        return EXIT_FAILURE;
      }

      com.func();

      break;
    }
  }

#ifdef FANCON_PROFILE
  HeapProfilerStop();
//  ProfilerStop();
#endif // FANCON_PROFILE

  return EXIT_SUCCESS;
}
