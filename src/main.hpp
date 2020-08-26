#ifndef FANCON_MAIN_HPP
#define FANCON_MAIN_HPP

#ifdef FANCON_PROFILE
#include <gperftools/heap-profiler.h>
#include <gperftools/profiler.h>
#endif // FANCON_PROFILE

#include <iostream>
#include <regex>
#include <sstream>

#include "Args.hpp"
#include "Client.hpp"
#include "Service.hpp"

using fc::Args;
using fc::Client;
using fc::Util::is_root;
using std::system;

int main(int argc, char *argv[]);

namespace fc {
Args &read_args(int argc, char **argv, Args &args);
void print_args(Args &args);
void signal_handler(int signal);
void register_signal_handler();
} // namespace fc

#endif // FANCON_MAIN_HPP
