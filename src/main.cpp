#include "main.hpp"

int main(int argc, char *argv[]) {
  using namespace fc;
#ifdef FANCON_PROFILE
  ProfilerStart("fancon_main");
  HeapProfilerStart("fancon_main");
#endif // FANCON_PROFILE

  args_map args{{"help", "0"},
                {"stop", "0"},
                {"reload", "0"},
                {"test", "0"},
                {"config", config_path_default},
                {"log-lvl", "info"},
                {"daemonize", "0"},
                {"system-info", "0"}};
  read_args(argc, argv, args);

  set_log_level(args["log-lvl"]);

  if (to_bool(args["help"])) {
    print_help();
    return EXIT_SUCCESS;
  }

  if (!is_root())
    return EXIT_FAILURE;

  if (to_bool(args["system-info"])) {
    return (save_system_info()) ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  // Only allow one instance
  auto [lock, only_instance] = instance_check();
  const bool stop = to_bool(args["stop"]), reload = to_bool(args["reload"]);

  if (!only_instance) {
    if (stop) {
      stop_instances();
    } else if (reload) {
      reload_instances();
    } else {
      LOG(llvl::warning) << "Only one instance can be run" << fc::log::flush;
      sleep_for(milliseconds(50)); // Allow time for the warning to flush
      offer_trailing_journal();
    }

    return EXIT_SUCCESS;
  } else if (stop || reload) {
    LOG(llvl::warning) << "Not running";
    return EXIT_SUCCESS;
  }

  if (to_bool(args["daemonize"]))
    daemonize();

  register_signal_handler();
  try {
    fc::Controller con(args["config"]);

    if (const bool test_safely = to_bool(args["test_safely"]);
        to_bool(args["test"]) || test_safely) {
      con.test(true, test_safely);
    }

    con.start();
  } catch (std::exception &e) {
    LOG(llvl::fatal) << e.what();
    throw e;
  }

#ifdef FANCON_PROFILE
  HeapProfilerStop();
  ProfilerStop();
#endif // FANCON_PROFILE

  return EXIT_SUCCESS;
}

args_map &fc::read_args(int argc, char *argv[], args_map &args) {
  args_map short_to_args{
      {"h", "help"},    {"s", "stop"},         {"r", "reload"},
      {"t", "test"},    {"ts", "test-safely"}, {"c", "config"},
      {"l", "log-lvl"}, {"d", "daemonize"},    {"i", "system-info"}};

  const std::regex arg_regex(R"(-*([^\s=]+)[='"]*([^\s='"]+)?['"]*)");
  for (int i = 1; i < argc; ++i) {
    for (char *p = argv[i]; *p; ++p)
      *p = static_cast<char>(tolower(*p));
    const string input = string(argv[i]);
    std::smatch match;
    std::regex_match(input, match, arg_regex);

    if (match[1].matched) { // match 1: arg; 2: arg's value
      string arg{match[1].str()};
      if (const auto it = short_to_args.find(arg); it != short_to_args.end())
        arg = it->second;

      if (args.count(arg) > 0) {
        args[arg] = (match[2].matched) ? match[2].str() : "1";
      } else {
        LOG(llvl::warning) << "Unknown argument '" << arg
                           << "' from statement '" << match.str() << "'";
      }
    }
  }

  if (fc::debugging())
    print_args(args);

  return args;
}

void fc::print_args(args_map &args) {
  std::stringstream ss;
  ss << "Started with arguments: [";
  for (auto it = args.begin(); it != args.end();) {
    ss << it->first << ": " << it->second
       << ((++it != args.end()) ? ", " : "]");
  }
  std::cout << ss.str() << endl;
}

bool fc::to_bool(const string &str) {
  return str == "1" || str == "true" || str == "yes";
}

void fc::print_help() {
  const char *s = "                   "; // Enough spaces to embed the next line
  LOG(llvl::info)
      << "fancon argument=value ..." << endl
      << "-h  help         Show this help" << endl
      << "    start        Start the fan controller (default: true)" << endl
      << "-s  stop         Stop the fan controller" << endl
      << "-r  reload       Forces the fan controller to reload all data" << endl
      << "-t  test         Force tests of all fans found" << endl
      << "-ts test-safely  Test fans one at a time to avoid stopping all "
      << "fans at once (default: false)" << endl
      << s << "at once, avoiding high temps but significantly increasing"
      << " duration" << endl
      << "-c  config       Configuration file (default: " << log::output_bold
      << config_path_default << log::output_reset << ")" << endl
      << "-l  log-lvl      Sets the logging level: info, debug, trace,"
      << " warning, error (default: info)" << endl
      << "-d  daemonize    Daemonize the process (default: false)" << endl
      << "-i  system-info  Save system info to the current directory" << endl
      << s << "(file name default: " << hardware_info_path_default << ")"
      << endl;
}

bool fc::is_root() {
  if (getuid() != 0) {
    LOG(llvl::fatal) << "Must be run as root";
    return false;
  }
  return true;
}

void fc::set_log_level(const string &log_lvl) {
  const optional<llvl> l = fc::log::str_to_log_level(log_lvl);
  if (!l)
    LOG(llvl::warning) << "Invalid log-lvl: " << log_lvl;

  fc::log::set_level(l.value_or(llvl::debug));
}

tuple<file_lock, bool> fc::instance_check() {
  // Use a separate lock file to avoid race conditions
  // file unlocks after being written to
  const path lock_path("/run/fancon.lock");
  if (!exists(lock_path))
    Util::write(lock_path, "");

  // An instance is running if the file is already locked
  file_lock lock(lock_path.c_str());
  if (!lock.try_lock()) {
    LOG(llvl::debug) << "Running instance pid: " << Util::read<pid_t>(pid_path);
    return {move(lock), false};
  }

  Util::write(pid_path, to_string(getpid()) + "\n");
  return {move(lock), true};
}

void fc::stop_instances() {
  const auto pid = Util::read_<pid_t>(pid_path);
  if (!pid)
    return;

  kill(*pid, SIGQUIT);
  int status;
  waitpid(*pid, &status, WUNTRACED | WCONTINUED);
  //  return (WIFEXITED(status)) ? EXIT_SUCCESS : EXIT_FAILURE;
}

void fc::reload_instances() {
  const auto pid = Util::read_<pid_t>(pid_path);
  if (pid)
    kill(*pid, SIGUSR1);
}

void fc::offer_trailing_journal() {
  // Only recommend if it's not systemd running it!
  // is-active returns 0 if the service is active
  if (fc::is_systemd() || system("systemctl is-active --quiet fancon") != 0)
    return;

  if (!isatty(STDIN_FILENO))
    return;

  cout << "Trail (systemd) daemon log? (y/n): ";
  string input;
  std::cin >> input;
  if (std::tolower(input.front()) != 'y')
    return;

  cout << fc::log::output_bold_red << "Press ctrl-C to exit"
       << fc::log::output_reset << endl;
  [[maybe_unused]] const auto r = system("journalctl -b -fu fancon");
}

void fc::daemonize() {
  const auto fork_thread = []() {
    pid_t pid = fork();

    // On success: child's PID is returned in parent, 0 returned in child
    if (pid >= 0)
      exit(EXIT_SUCCESS);
    else if (pid == -1) { // On failure: -1 returned in parent
      LOG(llvl::fatal) << "Failed to fork off parent";
      exit(EXIT_FAILURE);
    }
  };

  // Daemonize process by forking, creating new session, then forking again
  fork_thread();

  // Create a new session for the child
  if (setsid() < 0) {
    LOG(llvl::fatal) << "Failed to fork off parent";
    exit(EXIT_FAILURE);
  }

  fork_thread();

  // Redirect standard file descriptors to /dev/null
  const char *dnull = "/dev/null";
  stdin = fopen(dnull, "r");
  stdout = fopen(dnull, "r+");
  stderr = fopen(dnull, "r+");

  // Set an appropriate rw umask
  // 664 (rw-rw-r--) files; 775 (rwxrwxr-x) directories
  umask(002);

  // Set working dir to /
  if (chdir("/") < 0)
    LOG(llvl::error) << "Failed to set working directory to '/'";
}

void fc::print_directory(const path &dir, std::ostream &os, uint depth) {
  static std::set<path> searched_symlinks;
  for (const auto &e : fs::recursive_directory_iterator(dir)) {
    os << e;
    // TODO: check the file is readable?
    if (e.is_regular_file()) {
      const string contents = Util::read_line(e).value_or("read failed");
      os << ": " << contents;
    }
    os << endl;

    // Recurse into only the most shallow symlinks
    if (e.is_symlink() && depth == 0) {
      if (const path p = fs::read_symlink(e); searched_symlinks.count(p) == 0) {
        searched_symlinks.emplace(p);
        print_directory(e, os, depth + 1);
      }
    }
  }
}

bool fc::save_system_info() {
  const path output_path = fs::current_path() / hardware_info_path_default;
  LOG(llvl::info) << "Saving hardware info to " << output_path;
  std::ofstream ofs(output_path.c_str());
  if (!ofs)
    return false;

  // Save devices info gathered
  fc::Devices devices(true);
  fc_pb::Devices devices_pb;
  devices.to(devices_pb);

  string out_s;
  google::protobuf::TextFormat::PrintToString(devices_pb, &out_s);
  ofs << out_s << endl;
  if (!ofs)
    return false;

  // Save /sys/class/hwmon/*
  const path hwmon_path("/sys/class/hwmon");
  if (exists(hwmon_path)) {
    print_directory(hwmon_path, ofs);
  } else {
    LOG(llvl::error) << hwmon_path << " doesn't exist!" << endl;
    ofs << hwmon_path << " doesn't exist!" << endl;
  }

  // Allow all users to read & write to the file
  const auto perms = fs::perms::others_read | fs::perms::others_write;
  fs::permissions(output_path, perms, fs::perm_options::add);

  return true;
}

void fc::signal_handler(int signal) {
  switch (signal) {
  case SIGINT:
  case SIGQUIT:
  case SIGTERM:
    fc::Controller::stop();
    break;
  case SIGUSR1:
    fc::Controller::reload();
    break;
  default:
    LOG(llvl::warning) << "Unhandled signal (" << signal
                       << "): " << strsignal(signal);
  }
}

void fc::register_signal_handler() {
  for (const auto &s : {SIGINT, SIGQUIT, SIGTERM, SIGUSR1})
    std::signal(s, &signal_handler);
}
