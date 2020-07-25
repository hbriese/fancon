#ifndef FANCON_SRC_CLIENT_HPP
#define FANCON_SRC_CLIENT_HPP

#include "Args.hpp"
#include "Util.hpp"
#include "proto/DevicesSpec.grpc.pb.h"
#include "proto/DevicesSpec.pb.h"
#include <grpcpp/grpcpp.h>

using fc::Args;
using fc_pb::Empty;
using grpc::ClientContext;
using grpc::Status;
using grpc::StatusCode;

namespace fc {
class Client {
public:
  Client();

  void run(Args &args);

  void stop_service();
  optional<fc_pb::Devices> get_devices();
  optional<fc_pb::Devices> get_enumerated_devices();

  void status();
  void enable(const string &flabel);
  void enable();
  void disable(const string &flabel);
  void disable();
  void test(bool forced);
  void test(const string &flabel, bool forced);
  void reload();
  void nv_init();
  void sysinfo(const string &p);

  static void print_help(const string &conf);
  static bool service_running();

  explicit operator bool() const;

private:
  unique_ptr<fc_pb::DService::Stub> client;
  shared_ptr<grpc::Channel> channel;
  fc_pb::Empty empty;

  bool connected(long timeout_ms) const;
  static bool check(const grpc::Status &status);
  static void log_service_unavailable();
  static void enumerate_directory(const path &dir, std::ostream &os,
                                  uint depth = 0);
  static string status_text(fc_pb::FanStatus_Status status);
  static fc_pb::FanLabel from(const string &flabel);
};
} // namespace fc

#endif // FANCON_SRC_CLIENT_HPP
