#ifndef FANCON_SRC_CLIENT_HPP
#define FANCON_SRC_CLIENT_HPP

#include "Args.hpp"
#include "Devices.hpp"
#include "Util.hpp"
#include "proto/DevicesSpec.grpc.pb.h"
#include "proto/DevicesSpec.pb.h"
#include <grpcpp/grpcpp.h>

using fc::Args;
using fc_pb::ControllerState;
using fc_pb::Empty;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;

namespace fc {
class Client {
public:
  Client();

  void run(Args &args);
  optional<ControllerState> Status();
  bool Enable();
  bool Disable();
  bool Reload();
  bool NvInit();
  optional<fc::Devices> GetDevices();
  optional<fc::Devices> GetEnumeratedDevices();
  bool Test(bool forced);
  bool Test(const string &fan_label, bool forced);
  bool StopService();
  bool Sysinfo(const string &p);

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
};
} // namespace fc

#endif // FANCON_SRC_CLIENT_HPP
