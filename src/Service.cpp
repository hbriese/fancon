#include "Service.hpp"

fc::Service::Service(const path &config_path, bool daemon)
    : controller(config_path) {
  if (daemon)
    daemonize();
}

fc::Service::~Service() { shutdown(); }

void fc::Service::run() {
  try {
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
      controller.enable_all();
      server->Wait();
    }
  } catch (std::exception &e) {
    LOG(llvl::fatal) << e.what();
    throw e;
  }
}

void fc::Service::shutdown() {
  controller.disable_all();
  if (server)
    server->Shutdown(Util::deadline(250));
}

Status fc::Service::StopService([[maybe_unused]] ServerContext *context,
                                [[maybe_unused]] const fc_pb::Empty *e,
                                [[maybe_unused]] fc_pb::Empty *resp) {
  thread([&] { shutdown(); }).detach();
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
  controller.set_devices(*devices);
  return Status::OK;
}

Status fc::Service::SubscribeDevices([[maybe_unused]] ServerContext *context,
                                     [[maybe_unused]] const fc_pb::Empty *e,
                                     ServerWriter<fc_pb::Devices> *writer) {
  auto cb = [&](const fc::Devices &devices) {
    if (!context->IsCancelled()) {
      fc_pb::Devices resp;
      devices.to(resp);
      writer->Write(resp);
    }
  };

  // Send the initial state
  cb(controller.devices);

  // Register the listener, sending status on changes
  const auto it =
      controller.device_observers.insert(controller.device_observers.end(), cb);

  while (!context->IsCancelled())
    sleep_for(sleep_interval);

  // Acquire a removal lock before removing to ensure it's not in use
  controller.device_observers_mutex.acquire_removal_lock();
  controller.device_observers.erase(it);

  return Status::OK;
}

Status
fc::Service::GetEnumeratedDevices([[maybe_unused]] ServerContext *context,
                                  [[maybe_unused]] const fc_pb::Empty *req,
                                  fc_pb::Devices *devices) {
  Devices(true, true).to(*devices);
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

Status fc::Service::GetFanStatus([[maybe_unused]] ServerContext *context,
                                 const fc_pb::FanLabel *l,
                                 fc_pb::FanStatus *status) {
  const auto fit = controller.devices.fans.find(l->label());
  if (fit == controller.devices.fans.end())
    return Status(StatusCode::NOT_FOUND, l->label());

  status->set_label(l->label());
  status->set_status(controller.status(l->label()));
  status->set_rpm(fit->second->get_rpm());
  status->set_pwm(fit->second->get_pwm());
  return Status::OK;
}

Status fc::Service::SubscribeFanStatus([[maybe_unused]] ServerContext *context,
                                       [[maybe_unused]] const fc_pb::Empty *e,
                                       ServerWriter<fc_pb::FanStatus> *writer) {
  mutex m;
  const auto cb = [&](const FanInterface &f, const FanStatus status) {
    if (!context->IsCancelled()) {
      fc_pb::FanStatus resp;
      resp.set_label(f.label);
      resp.set_status(status);
      resp.set_rpm(f.get_rpm());
      resp.set_pwm(f.get_pwm());
      const lock_guard lg(m);
      if (!context->IsCancelled())
        writer->Write(resp);
    }
  };

  // Send the initial status
  for (const auto &[flabel, f] : controller.devices.fans)
    cb(*f, controller.status(flabel));

  // Register the listener, sending status on changes
  const auto cb_it =
      controller.status_observers.insert(controller.status_observers.end(), cb);

  while (!context->IsCancelled())
    sleep_for(sleep_interval);

  // Acquire a removal lock before removing to ensure it's not in use
  controller.status_observers_mutex.acquire_removal_lock();
  controller.status_observers.erase(cb_it);

  return Status::OK;
}

Status fc::Service::Enable([[maybe_unused]] ServerContext *context,
                           const fc_pb::FanLabel *l,
                           [[maybe_unused]] fc_pb::Empty *e) {
  auto it = controller.devices.fans.find(l->label());
  if (it == controller.devices.fans.end())
    return Status(StatusCode::NOT_FOUND, l->label());

  controller.enable(*it->second);
  return Status::OK;
}

Status fc::Service::EnableAll([[maybe_unused]] ServerContext *context,
                              [[maybe_unused]] const fc_pb::Empty *e,
                              [[maybe_unused]] fc_pb::Empty *resp) {
  controller.enable_all();
  return Status::OK;
}

Status fc::Service::Disable([[maybe_unused]] ServerContext *context,
                            const fc_pb::FanLabel *l,
                            [[maybe_unused]] fc_pb::Empty *resp) {
  if (!controller.devices.fans.contains(l->label()))
    return Status(StatusCode::NOT_FOUND, l->label());

  controller.disable(l->label());
  return Status::OK;
}

Status fc::Service::DisableAll([[maybe_unused]] ServerContext *context,
                               [[maybe_unused]] const fc_pb::Empty *e,
                               [[maybe_unused]] fc_pb::Empty *resp) {
  controller.disable_all();
  return Status::OK;
}

Status fc::Service::Test([[maybe_unused]] ServerContext *context,
                         const fc_pb::TestRequest *e,
                         ServerWriter<fc_pb::TestResponse> *writer) {
  auto it = controller.devices.fans.find(e->device_label());
  if (it == controller.devices.fans.end())
    return Status(StatusCode::NOT_FOUND, e->device_label());

  auto cb = [&](int &status) {
    fc_pb::TestResponse resp;
    resp.set_status(status);
    if (status == 100)
      writer->WriteLast(resp, grpc::WriteOptions());
    else
      writer->Write(resp);
  };

  controller.test(*it->second, e->forced(), cb);

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
