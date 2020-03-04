#ifndef FANCON_MAIN_HPP
#define FANCON_MAIN_HPP

#ifndef FANCON_SYSCONFDIR
#define FANCON_SYSCONFDIR "/etc"
#endif // FANCON_SYSCONFDIR

#ifdef FANCON_PROFILE
#include <gperftools/heap-profiler.h>
#include <gperftools/profiler.h>
#endif // FANCON_PROFILE

#include <boost/interprocess/sync/file_lock.hpp>
#include <csignal>
#include <future>
#include <iostream>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <sys/wait.h>

#include "Controller.hpp"
#include "Service.hpp"

using boost::interprocess::file_lock;
using std::system;

using args_map = std::map<string, string>;

int main(int argc, char *argv[]);

namespace fc {
const string config_path_default(FANCON_SYSCONFDIR "/fancon.conf");
const path pid_path{"/run/fancon.pid"};
const char *hardware_info_path_default = "fancon_system_info.txt";

args_map &read_args(int argc, char **argv, args_map &args);
void print_args(args_map &args);
bool to_bool(const string &str);
void print_help();

bool is_root();
void set_log_level(const string &log_lvl);
tuple<file_lock, bool> instance_check();
void stop_instances();
void reload_instances();
void offer_trailing_journal();
void daemonize();
void print_directory(const path &dir, std::ostream &os, uint depth = 0);
bool save_system_info();

void signal_handler(int signal);
void register_signal_handler();
} // namespace fc

#endif // FANCON_MAIN_HPP
