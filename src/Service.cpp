#include "Service.hpp"

fc::Service::Service(const path &config_path, bool daemon)
    : controller(config_path) {
  if (daemon)
    daemonize();
}

fc::Service::~Service() {
  disable_controller();
  if (server)
    server->Shutdown();
}

void fc::Service::run() {
  try {
    if (service_running()) {
      LOG(llvl::fatal) << "Only 1 instance may be run";
      return;
    }

    // TODO: use SSL
    // auto ssl_options = grpc::SslServerCredentialsOptions();
    // ...
    // auto creds = grpc::SslServerCredentials(ssl_options);

    auto creds = grpc::InsecureServerCredentials();
    ServerBuilder builder;
    server = builder.AddListeningPort(SERVICE_ADDR, creds)
                 .RegisterService(this)
                 .BuildAndStart();

    if (server) {
      enable_controller();
      server->Wait();
    }
  } catch (std::exception &e) {
    LOG(llvl::fatal) << e.what();
    throw e;
  }
}

void fc::Service::enable_controller() {
  if (controller.disabled())
    controller_thread = thread([this] { controller.enable(); });
}

void fc::Service::disable_controller() {
  controller.disable();
  controller_thread.interrupt();
}

Status fc::Service::StopService([[maybe_unused]] ServerContext *context,
                                [[maybe_unused]] const fc_pb::Empty *e,
                                [[maybe_unused]] fc_pb::Empty *resp) {
  disable_controller();
  thread([&] { server->Shutdown(); }).detach();
  return Status::OK;
}

Status fc::Service::Enable([[maybe_unused]] ServerContext *context,
                           [[maybe_unused]] const fc_pb::Empty *e,
                           [[maybe_unused]] fc_pb::Empty *resp) {
  enable_controller();
  return Status::OK;
}

Status fc::Service::Disable([[maybe_unused]] ServerContext *context,
                            [[maybe_unused]] const fc_pb::Empty *e,
                            [[maybe_unused]] fc_pb::Empty *resp) {
  disable_controller();
  return Status::OK;
}

Status fc::Service::Reload([[maybe_unused]] ServerContext *context,
                           [[maybe_unused]] const fc_pb::Empty *e,
                           [[maybe_unused]] fc_pb::Empty *resp) {
  controller.reload();
  return Status::OK;
}

Status fc::Service::NvInit([[maybe_unused]] ServerContext *context,
                           [[maybe_unused]] const fc_pb::Empty *e,
                           [[maybe_unused]] fc_pb::Empty *resp) {
  controller.nv_init();
  return Status::OK;
}

Status fc::Service::ControllerStatus([[maybe_unused]] ServerContext *context,
                                     [[maybe_unused]] const fc_pb::Empty *e,
                                     fc_pb::ControllerState *resp) {
  auto s = static_cast<fc_pb::ControllerState_State>(controller.state);
  resp->set_state(s);
  return Status::OK;
}

Status fc::Service::GetDevices([[maybe_unused]] ServerContext *context,
                               [[maybe_unused]] const fc_pb::Empty *e,
                               fc_pb::Devices *devices) {
  controller.devices.to(*devices);
  return Status::OK;
}

Status fc::Service::SetDevices([[maybe_unused]] ServerContext *context,
                               const fc_pb::Devices *devices,
                               [[maybe_unused]] fc_pb::Empty *e) {
  controller.devices = fc::Devices();
  controller.devices.from(*devices);
  return Status::OK;
}

Status
fc::Service::GetEnumeratedDevices([[maybe_unused]] ServerContext *context,
                                  [[maybe_unused]] const fc_pb::Empty *req,
                                  fc_pb::Devices *devices) {
  Devices(true).to(*devices);
  return Status::OK;
}

Status fc::Service::Test([[maybe_unused]] ServerContext *context,
                         const fc_pb::TestRequest *e,
                         ServerWriter<fc_pb::TestResponse> *writer) {
  auto fit = controller.devices.fans.find(e->device_label());
  if (fit == controller.devices.fans.end())
    // TODO: error << device isn't found
    return Status::OK;

  auto cb = [&](int &status) {
    fc_pb::TestResponse resp;
    resp.set_status(status);
    if (status == 100)
      writer->WriteLast(resp, grpc::WriteOptions());
    else
      writer->Write(resp);
  };

  controller.test(*fit->second, e->forced(), cb);

  return Status::OK;
}

Status fc::Service::GetControllerConfig([[maybe_unused]] ServerContext *context,
                                        [[maybe_unused]] const fc_pb::Empty *e,
                                        fc_pb::ControllerConfig *config) {
  controller.to(*config);
  return Status::OK;
}

Status fc::Service::SetControllerConfig([[maybe_unused]] ServerContext *context,
                                        const fc_pb::ControllerConfig *config,
                                        [[maybe_unused]] fc_pb::Empty *e) {
  controller.from(*config);
  controller.reload();
  return Status::OK;
}

void fc::Service::daemonize() {
  const auto fork_thread = []() {
    pid_t pid = fork();

    // On success: child's PID is returned in parent, 0 returned in child
    if (pid >= 0)
      exit(EXIT_SUCCESS);
    else if (pid == -1) { // On failure: -1 returned in parent
      LOG(llvl::fatal) << "Failed to fork off parent";
      exit(EXIT_FAILURE);
    }
  };

  // Daemonize process by forking, creating new session, then forking again
  fork_thread();

  // Create a new session for the child
  if (setsid() < 0) {
    LOG(llvl::fatal) << "Failed to fork off parent";
    exit(EXIT_FAILURE);
  }

  fork_thread();

  // Redirect standard file descriptors to /dev/null
  const char *dnull = "/dev/null";
  stdin = fopen(dnull, "r");
  stdout = fopen(dnull, "r+");
  stderr = fopen(dnull, "r+");

  // 664 (rw-rw-r--) files; 775 (rwxrwxr-x) directories
  umask(002);

  // Set working dir to /
  if (chdir("/") < 0)
    LOG(llvl::error) << "Failed to set working directory to '/'";
}

bool fc::Service::service_running() {
  auto creds = grpc::InsecureChannelCredentials();
  auto channel = grpc::CreateChannel(Util::SERVICE_ADDR, creds);
  channel->WaitForConnected(Util::deadline(200));
  return channel->GetState(true) == GRPC_CHANNEL_READY;
}

// void fc::Service::signal_handler(int signal) {
//  switch (signal) {
//  case SIGINT:
//  case SIGQUIT:
//  case SIGTERM:
//    fc::Controller::disable();
//    SERVER->Shutdown();
//    break;
//  default:
//    LOG(llvl::warning) << "Unhandled signal (" << signal
//                       << "): " << strsignal(signal);
//  }
//}
//
// void fc::Service::register_signal_handler() {
//  for (const auto &s : {SIGINT, SIGQUIT, SIGTERM, SIGUSR1})
//    std::signal(s, &fc::Service::signal_handler);
//}
