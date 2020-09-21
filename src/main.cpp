#include "main.hpp"

int main(int argc, char *argv[]) {
  using namespace fc;
#ifdef FANCON_PROFILE
  ProfilerStart("fancon_main");
  HeapProfilerStart("fancon_main");
#endif // FANCON_PROFILE

  Args args;
  read_args(argc, argv, args);

  const path config_path(args.config.value);
  if (args.trace || args.verbose)
    log::set_level((args.trace) ? llvl::trace : llvl::debug);
  else
    log::set_level(llvl::info);

  // SERVICE
  if (args.service) {
    if (!is_root()) {
      LOG(llvl::fatal) << "Service must be run as root";
      return EXIT_FAILURE;
    }

    if (Client::service_running()) {
      LOG(llvl::fatal) << "Only 1 instance may be run";
      return EXIT_SUCCESS;
    }

    if (!is_systemd())
      LOG(llvl::info) << "Service started";

    fc::Service service(config_path, args.daemon);
    register_signal_handler();
    service.run();
    return EXIT_SUCCESS;
  }

  // CLIENT
  if (is_root() && !fc::is_systemd())
    LOG(llvl::warning) << "Running the client as root is not recommended";

  Client client;
  client.run(args);

#ifdef FANCON_PROFILE
  HeapProfilerStop();
  ProfilerStop();
#endif // FANCON_PROFILE

  return EXIT_SUCCESS;
}

Args &fc::read_args(int argc, char *argv[], Args &args) {
  // Transform arguments to lower
  for (int i = 1; i < argc; i++) {
    for (char *p = argv[i]; *p; ++p)
      *p = static_cast<char>(tolower(*p));
  }

  map<string, string> short_to_key = args.short_to_key();
  const std::regex rexpr(R"([\s-]*([\S]*)?[\s]*)");

  auto rmatch = [&](const string &s) {
    std::smatch match;
    std::regex_match(s, match, rexpr);

    string arg_str;
    auto arg_it = args.from_key.end();
    bool is_arg = false;
    if (match[1].matched) {
      arg_str = match[1].str();
      if (auto it = short_to_key.find(arg_str); it != short_to_key.end())
        arg_str = it->second;

      arg_it = args.from_key.find(arg_str);
      is_arg = arg_it != args.from_key.end();
    } else
      arg_str = match.str();

    return make_tuple(arg_str, arg_it, is_arg);
  };

  for (int i = 1; i < argc; ++i) {
    auto [arg_str, arg_it, is_arg] = rmatch(argv[i]);
    if (!is_arg) {
      LOG(llvl::error) << "Unknown argument: '" << arg_str << "'";
      continue;
    }

    Arg &arg = arg_it->second;
    arg.triggered = true;

    if (arg.potential_value) {
      auto check_needs_value = [&] {
        if (arg.needs_value && !arg.has_value()) {
          arg.triggered = false;
          LOG(llvl::error) << "Arg '" << arg.key << "' needs a value";
        }
      };

      // Check if arg[i+1] is the value
      if ((i + 1) < argc) {
        auto [val_str, val_it, val_is_arg] = rmatch(argv[i + 1]);
        if (!val_is_arg) {
          arg.value = val_str;
          i += 1;
        } else
          check_needs_value();
      } else
        check_needs_value();
    }
  }

  if (fc::debugging())
    print_args(args);

  return args;
}

void fc::print_args(Args &args) {
  std::stringstream ss;
  ss << "Started with arguments: [";
  for (auto it = args.from_key.begin(); it != args.from_key.end();) {
    ss << it->second.key << ": " << it->second.triggered
       << (!it->second.value.empty() ? (" (" + it->second.value + ")") : "")
       << ((++it != args.from_key.end()) ? ", " : "]");
  }
  cout << ss.str() << endl;
}

void fc::signal_handler(int signal) {
  switch (signal) {
  case SIGINT:
  case SIGQUIT:
  case SIGTERM:
    Client().stop_service();
    break;
  default:
    LOG(llvl::warning) << "Unhandled signal (" << signal
                       << "): " << strsignal(signal);
  }
}

void fc::register_signal_handler() {
  for (const auto &s : {SIGINT, SIGQUIT, SIGTERM, SIGUSR1})
    std::signal(s, &fc::signal_handler);
}
