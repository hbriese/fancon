#ifndef FANCON_SRC_ARGS_HPP_
#define FANCON_SRC_ARGS_HPP_

#ifndef FANCON_SYSCONFDIR
#define FANCON_SYSCONFDIR "/etc"
#endif // FANCON_SYSCONFDIR

#include "Util.hpp"

using fc::Util::is_root;

namespace fc {
static const char *DEFAULT_CONF_PATH(FANCON_SYSCONFDIR "/fancon.conf"),
    *DEFAULT_SYSINFO_PATH = "fancon_sysinfo.txt";

class Arg {
public:
  Arg(string name, string short_name = "", bool has_value = false,
      bool needs_value = false, string value = "", bool triggered = false);

  string key, short_key, value;
  bool triggered, potential_value, needs_value;

  bool has_value() const;
  operator bool() const;
};

class Args {
public:
  Arg help = {"help", "h"}, status = {"status", "s"}, enable = {"enable"},
      disable = {"disable"}, reload = {"reload"},
      test = {"test", "t", true, false}, force = {"force", "f"},
      config = {"config", "c", true, true, DEFAULT_CONF_PATH, true},
      service = {"service"}, daemon = {"daemon", "d"},
      stop_service = {"stop-service"},
      sysinfo = {"sysinfo", "i", true, true, DEFAULT_SYSINFO_PATH},
      nv_init = {"nv-init"}, verbose = {"verbose", "v"}, trace = {"trace", "a"};

  map<string, Arg &> from_key = {{help.key, help},
                                 {status.key, status},
                                 {enable.key, enable},
                                 {disable.key, disable},
                                 {reload.key, reload},
                                 {test.key, test},
                                 {force.key, force},
                                 {config.key, config},
                                 {service.key, service},
                                 {daemon.key, daemon},
                                 {stop_service.key, stop_service},
                                 {sysinfo.key, sysinfo},
                                 {nv_init.key, nv_init},
                                 {verbose.key, verbose},
                                 {trace.key, trace}};
};
} // namespace fc

#endif // FANCON_SRC_ARGS_HPP_
