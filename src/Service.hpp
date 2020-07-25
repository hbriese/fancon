#ifndef FANCON_SERVICE_HPP
#define FANCON_SERVICE_HPP

#include <csignal>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server.h>
#include <grpcpp/support/status.h>
#include <mutex>
#include <sys/stat.h>

#include "Controller.hpp"
#include "proto/DevicesSpec.grpc.pb.h"
#include "proto/DevicesSpec.pb.h"

using fc::Controller;
using fc::Util::SERVICE_ADDR;
using fc_pb::Empty;
using grpc::ChannelCredentials;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using GStatus = grpc::Status;
using GStatusCode = grpc::StatusCode;
using std::lock_guard;
using std::mutex;

namespace fc {
class Service : public fc_pb::DService::Service {
public:
  explicit Service(const path &config_path, bool daemon = false);
  ~Service() override;

  void run();

  GStatus StopService(ServerContext *context, const fc_pb::Empty *e,
                      fc_pb::Empty *resp) override;
  GStatus GetDevices(ServerContext *context, const fc_pb::Empty *e,
                     fc_pb::Devices *devices) override;
  GStatus SetDevices(ServerContext *context, const fc_pb::Devices *devices,
                     fc_pb::Empty *e) override;
  GStatus SubscribeDevices(ServerContext *context, const fc_pb::Empty *e,
                           ServerWriter<fc_pb::Devices> *writer) override;
  GStatus GetEnumeratedDevices(ServerContext *context, const fc_pb::Empty *req,
                               fc_pb::Devices *devices) override;
  GStatus GetControllerConfig(ServerContext *context, const fc_pb::Empty *e,
                              fc_pb::ControllerConfig *config) override;
  GStatus SetControllerConfig(ServerContext *context,
                              const fc_pb::ControllerConfig *config,
                              fc_pb::Empty *e) override;

  GStatus Status(ServerContext *context, const fc_pb::FanLabel *l,
                 fc_pb::FanStatus *status) override;
  GStatus Enable(ServerContext *context, const fc_pb::FanLabel *l,
                 fc_pb::Empty *resp) override;
  GStatus EnableAll(ServerContext *context, const fc_pb::Empty *e,
                    fc_pb::Empty *resp) override;
  GStatus Disable(ServerContext *context, const fc_pb::FanLabel *l,
                  fc_pb::Empty *resp) override;
  GStatus DisableAll(ServerContext *context, const fc_pb::Empty *e,
                     fc_pb::Empty *resp) override;
  GStatus Test(ServerContext *context, const fc_pb::TestRequest *e,
               ServerWriter<fc_pb::TestResponse> *writer) override;
  GStatus Reload(ServerContext *context, const fc_pb::Empty *e,
                 fc_pb::Empty *resp) override;
  GStatus NvInit(ServerContext *context, const fc_pb::Empty *e,
                 fc_pb::Empty *resp) override;

private:
  fc::Controller controller;
  unique_ptr<Server> server;

  static void daemonize();

  //  static void signal_handler(int signal);
  //  static void register_signal_handler();
};
} // namespace fc

#endif // FANCON_SERVICE_HPP
