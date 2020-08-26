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
using grpc::Status;
using grpc::StatusCode;
using std::lock_guard;
using std::mutex;

namespace fc {
class Service : public fc_pb::DService::Service {
public:
  explicit Service(const path &config_path, bool daemon = false);
  ~Service() override;

  void run();
  void shutdown();

  Status StopService(ServerContext *context, const fc_pb::Empty *e,
                     fc_pb::Empty *resp) override;
  Status GetDevices(ServerContext *context, const fc_pb::Empty *e,
                    fc_pb::Devices *devices) override;
  Status SetDevices(ServerContext *context, const fc_pb::Devices *devices,
                    fc_pb::Empty *e) override;
  Status SubscribeDevices(ServerContext *context, const fc_pb::Empty *e,
                          ServerWriter<fc_pb::Devices> *writer) override;
  Status GetEnumeratedDevices(ServerContext *context, const fc_pb::Empty *e,
                              fc_pb::Devices *devices) override;
  Status GetControllerConfig(ServerContext *context, const fc_pb::Empty *e,
                             fc_pb::ControllerConfig *config) override;
  Status SetControllerConfig(ServerContext *context,
                             const fc_pb::ControllerConfig *config,
                             fc_pb::Empty *e) override;

  Status GetFanStatus(ServerContext *context, const fc_pb::FanLabel *l,
                      fc_pb::FanStatus *status) override;
  Status SubscribeFanStatus(ServerContext *context, const fc_pb::Empty *e,
                            ServerWriter<fc_pb::FanStatus> *writer) override;
  Status Enable(ServerContext *context, const fc_pb::FanLabel *l,
                fc_pb::Empty *resp) override;
  Status EnableAll(ServerContext *context, const fc_pb::Empty *e,
                   fc_pb::Empty *resp) override;
  Status Disable(ServerContext *context, const fc_pb::FanLabel *l,
                 fc_pb::Empty *resp) override;
  Status DisableAll(ServerContext *context, const fc_pb::Empty *e,
                    fc_pb::Empty *resp) override;
  Status Test(ServerContext *context, const fc_pb::TestRequest *e,
              ServerWriter<fc_pb::TestResponse> *writer) override;
  Status Reload(ServerContext *context, const fc_pb::Empty *e,
                fc_pb::Empty *resp) override;
  Status NvInit(ServerContext *context, const fc_pb::Empty *e,
                fc_pb::Empty *resp) override;

private:
  fc::Controller controller;
  unique_ptr<Server> server;
  const milliseconds sleep_interval = milliseconds(500);

  static void daemonize();

  //  static void signal_handler(int signal);
  //  static void register_signal_handler();
};
} // namespace fc

#endif // FANCON_SERVICE_HPP
