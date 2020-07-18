#ifndef FANCON_SERVICE_HPP
#define FANCON_SERVICE_HPP

#include "Controller.hpp"
#include <csignal>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server.h>
#include <grpcpp/support/status.h>
#include <mutex>
#include <sys/stat.h>

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
using std::lock_guard;
using std::mutex;

namespace fc {
class Service : public fc_pb::DService::Service {
public:
  explicit Service(const path &config_path, bool daemon = false);
  ~Service() override;

  void run();

  Status StopService(ServerContext *context, const fc_pb::Empty *e,
                     fc_pb::Empty *resp) override;
  Status Enable(ServerContext *context, const fc_pb::Empty *e,
                fc_pb::Empty *resp) override;
  Status Disable(ServerContext *context, const fc_pb::Empty *e,
                 fc_pb::Empty *resp) override;
  Status Reload(ServerContext *context, const fc_pb::Empty *e,
                fc_pb::Empty *resp) override;
  Status NvInit(ServerContext *context, const fc_pb::Empty *e,
                fc_pb::Empty *resp) override;
  Status ControllerStatus(ServerContext *context, const fc_pb::Empty *e,
                          fc_pb::ControllerState *resp) override;

  Status GetDevices(ServerContext *context, const fc_pb::Empty *e,
                    fc_pb::Devices *devices) override;
  Status SetDevices(ServerContext *context, const fc_pb::Devices *devices,
                    fc_pb::Empty *e) override;
  Status GetEnumeratedDevices(ServerContext *context, const fc_pb::Empty *req,
                              fc_pb::Devices *devices) override;
  Status Test(ServerContext *context, const fc_pb::TestRequest *e,
              ServerWriter<fc_pb::TestResponse> *writer) override;

  Status GetControllerConfig(ServerContext *context, const fc_pb::Empty *e,
                             fc_pb::ControllerConfig *config) override;
  Status SetControllerConfig(ServerContext *context,
                             const fc_pb::ControllerConfig *config,
                             fc_pb::Empty *e) override;

private:
  fc::Controller controller;
  unique_ptr<Server> server;
  thread controller_thread;
  mutex test_writer_mutex;

  void enable_controller();
  void disable_controller();
  static void daemonize();

  static bool service_running();

  //  static void signal_handler(int signal);
  //  static void register_signal_handler();
};
} // namespace fc

#endif // FANCON_SERVICE_HPP
