#ifndef FANCON_SRC_ARGS_HPP_
#define FANCON_SRC_ARGS_HPP_

#ifndef FANCON_SYSCONFDIR
#define FANCON_SYSCONFDIR "/etc"
#endif // FANCON_SYSCONFDIR

#include "Util.hpp"

using fc::Util::is_root;

namespace fc {
static const char *DEFAULT_CONF_PATH(FANCON_SYSCONFDIR "/fancon.conf"),
    *DEFAULT_SYSINFO_PATH = "sysinfo.txt";

class Arg {
public:
  Arg(string name, string short_name = "", bool potential_value = false,
      bool needs_value = false, string value = "", bool triggered = false);

  string key, short_key, value;
  bool triggered, potential_value, needs_value;

  bool has_value() const;
  operator bool() const;
};

class Args {
public:
  Arg help = {"help", "h"}, status = {"status", "s"},
      enable = {"enable", "e", true, false},
      disable = {"disable", "d", true, false},
      test = {"test", "t", true, false}, force = {"force", "f"},
      monitor = {"monitor", "m", true, false}, reload = {"reload", "r"},
      config = {"config", "c", true, true, DEFAULT_CONF_PATH, true},
      service = {"service"}, daemon = {"daemon"},
      stop_service = {"stop-service"},
      sysinfo = {"sysinfo", "i", true, true, DEFAULT_SYSINFO_PATH},
      recover = {"recover"}, nv_init = {"nv-init"}, verbose = {"verbose", "v"},
      trace = {"trace", "a"};

  map<string, Arg &> from_key = {
      a(help),    a(status),       a(enable),  a(disable), a(test),
      a(force),   a(monitor),      a(reload),  a(config),  a(service),
      a(daemon),  a(stop_service), a(sysinfo), a(recover), a(nv_init),
      a(verbose), a(trace)};

  map<string, string> short_to_key() const;

private:
  static pair<string, Arg &> a(Arg &arg);
};
} // namespace fc

#endif // FANCON_SRC_ARGS_HPP_
