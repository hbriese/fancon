#ifndef FANCON_MAIN_HPP
#define FANCON_MAIN_HPP

#ifdef FANCON_PROFILE
#include <gperftools/heap-profiler.h>
#include <gperftools/profiler.h>
#endif // FANCON_PROFILE

#include <boost/interprocess/sync/file_lock.hpp>
#include <csignal>
#include <future>
#include <grpcpp/channel.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <sys/wait.h>

#include "Args.hpp"
#include "Client.hpp"
#include "Service.hpp"

using boost::interprocess::file_lock;
using fc::Args;
using fc::Client;
using fc::Util::is_root;
using std::system;

int main(int argc, char *argv[]);

namespace fc {
Args &read_args(int argc, char **argv, Args &args);
void print_args(Args &args);

// tuple<file_lock, bool> instance_check();
// void stop_instances();
// void reload_instances();
// void nv_init();
// void offer_trailing_journal();
// void print_directory(const path &dir, std::ostream &os, uint depth = 0);
// bool save_system_info(const path &config_path);
} // namespace fc

#endif // FANCON_MAIN_HPP
