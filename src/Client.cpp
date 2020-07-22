#include "Client.hpp"
#include "Controller.hpp"

fc::Client::Client() {
  //  auto creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
  auto creds = grpc::InsecureChannelCredentials();
  channel = grpc::CreateChannel(Util::SERVICE_ADDR, creds);
  client = fc_pb::DService::NewStub(channel);

  if (is_root() && !fc::is_systemd())
    LOG(llvl::warning) << "Running the client as root is not recommended";
}

void fc::Client::run(Args &args) {
  if (args.help) {
    print_help(args.config.value);
    return;
  }

  if (!connected(1000)) {
    log_service_unavailable();
    return;
  }

  if (args.status) {
    const auto status = Status();
    if (status) {
      string s;
      if (*status == fc::ControllerState::ENABLED)
        s = "enabled";
      else if (*status == fc::ControllerState::DISABLED)
        s = "disabled";
      else if (*status == fc::ControllerState::RELOAD)
        s = "reload";
      LOG(llvl::info) << "Status: " << s;
    } else
      LOG(llvl::info) << "Status: service offline";
  } else if (args.enable) {
    if (Enable())
      LOG(llvl::info) << "Status: enabled";
  } else if (args.disable) {
    if (Disable())
      LOG(llvl::info) << "Status: disabled";
  } else if (args.reload) {
    if (Reload())
      LOG(llvl::info) << "Status: reload";
  } else if (args.nv_init) {
    NvInit();
  } else if (args.test) {
    if (!args.test.value.empty())
      Test(args.test.value, args.force);
    else
      Test(args.force);
  } else if (args.stop_service) {
    if (!StopService())
      LOG(llvl::error) << "Failed to stop service";
  } else if (args.sysinfo) {
    if (Sysinfo(args.sysinfo.value))
      LOG(llvl::info) << "Sysinfo written to: " << args.sysinfo.value;
    else
      LOG(llvl::error) << "Failed to write sysinfo";
  } else if (Util::is_atty() && !exists(args.config.value)) {
    // Offer test
    cout << log::fmt_green << "Test devices & generate a config? (y/n): ";
    char answer;
    std::cin >> answer;
    if (answer == 'y') {
      Test(args.force);
    }
  } else if (!args.help) { // else, excluding help which has already run
    print_help(args.config.value);
  }
}

optional<fc::ControllerState> fc::Client::Status() {
  ClientContext context;
  fc_pb::ControllerState resp;

  grpc::Status status = client->ControllerStatus(&context, empty, &resp);
  if (!check(status))
    return nullopt;

  return static_cast<fc::ControllerState>(resp.state());
}

bool fc::Client::Enable() {
  ClientContext context;
  return check(client->Enable(&context, empty, &empty));
}

bool fc::Client::Disable() {
  ClientContext context;
  return check(client->Disable(&context, empty, &empty));
}

bool fc::Client::Reload() {
  ClientContext context;
  return check(client->Reload(&context, empty, &empty));
}

bool fc::Client::NvInit() {
  ClientContext context;
  return check(client->NvInit(&context, empty, &empty));
}

optional<fc::Devices> fc::Client::GetDevices() {
  ClientContext context;
  fc_pb::Devices resp;

  grpc::Status status = client->GetDevices(&context, empty, &resp);
  if (!check(status))
    return nullopt;

  fc::Devices devices;
  devices.from(resp);

  return devices;
}

optional<fc::Devices> fc::Client::GetEnumeratedDevices() {
  ClientContext context;
  fc_pb::Devices resp;

  grpc::Status status = client->GetEnumeratedDevices(&context, empty, &resp);
  if (!check(status))
    return nullopt;

  fc::Devices devices;
  devices.from(resp);

  return devices;
}

bool fc::Client::Test(bool forced) {
  ClientContext context;
  mutex write_mutex;
  vector<thread> threads;
  vector<future<bool>> results;

  for (const auto &fan : {"hwmon3/fan1", "hwmon3/fan2"}) {
    results.emplace_back(std::async([&, fan] {
      fc_pb::TestRequest req;
      req.set_device_label(fan);
      req.set_forced(forced);

      auto reader = client->Test(&context, req);
      fc_pb::TestResponse resp;
      while (reader->Read(&resp)) {
        const lock_guard<mutex> lock(write_mutex);
        LOG(llvl::info) << fan << ": " << resp.status() << "%";
      }
      return check(reader->Finish());
    }));
  }

  for (auto &r : results) {
    if (r.valid() && !r.get())
      return false;
  }

  return true;
}

bool fc::Client::Test(const string &fan_label, bool forced) {
  ClientContext context;
  fc_pb::TestRequest req;
  req.set_device_label(fan_label);
  req.set_forced(forced);

  auto reader = client->Test(&context, req);
  fc_pb::TestResponse resp;
  while (reader->Read(&resp)) {
    LOG(llvl::info) << fan_label << ": " << resp.status() << "%";
  }
  const auto status = reader->Finish();

  const bool success = check(status);
  if (!success)
    LOG(llvl::error) << fan_label << ": failed to start test";
  return success;
}

bool fc::Client::StopService() {
  ClientContext context;
  return check(client->StopService(&context, empty, &empty));
}

bool fc::Client::Sysinfo(const string &p) {
  string out;
  std::ofstream ofs(p);

  ClientContext context;
  ofs << "Enumerated:" << endl;
  fc_pb::Devices enumerated;
  if (check(client->GetEnumeratedDevices(&context, empty, &enumerated))) {
    google::protobuf::TextFormat::PrintToString(enumerated, &out);
    ofs << out;
  }

  ClientContext context_user;
  ofs << endl << "User:" << endl;
  fc_pb::Devices user;
  if (check(client->GetDevices(&context_user, empty, &user))) {
    google::protobuf::TextFormat::PrintToString(user, &out);
    ofs << out;
  }

  const path hwmon_dir = "/sys/class/hwmon";
  ofs << endl << hwmon_dir.string() << ":" << endl;
  enumerate_directory(hwmon_dir, ofs);

  // TODO: logs

  if (!ofs)
    return false;

  // Allow all users to read & write to the file
  const auto perms = fs::perms::others_read | fs::perms::others_write;
  fs::permissions(p, perms, fs::perm_options::add);

  return true;
}

void fc::Client::print_help(const string &conf) {
  LOG(llvl::info) << "fancon arg [value] ..." << endl
                  << "h  help           Show this help" << endl
                  << "s  status         Status of the controller" << endl
                  << "   enable         Enable controller (default: true)"
                  << endl
                  << "   disable        Disable  controller" << endl
                  << "   reload         Reload config" << endl
                  << "t  test           Test ALL (untested) fans" << endl
                  << "t  test   [fan]   Test the given fan" << endl
                  << "f  force          Test even already tested fans "
                  << "(default: false)" << endl
                  << "c  config [file]  Config path (default: "
                  << log::fmt_green_bold << conf << log::fmt_reset << ")"
                  << endl
                  << "   service        Start as service" << endl
                  << "d  daemon         Daemonize the process (default: false)"
                  << endl
                  << "   stop-service   Stop the service" << endl
                  << "i  sysinfo [file] Save system info to file (default: "
                  << fc::DEFAULT_SYSINFO_PATH << ")" << endl
                  << "   nv-init        Init nvidia devices" << endl
                  << "v  verbose        Debug logging level" << endl
                  << "a  trace          Trace logging level" << endl;
}

bool fc::Client::service_running() {
  auto creds = grpc::InsecureChannelCredentials();
  auto channel = grpc::CreateChannel(Util::SERVICE_ADDR, creds);
  channel->WaitForConnected(Util::deadline(200));
  return channel->GetState(true) == GRPC_CHANNEL_READY;
}

fc::Client::operator bool() const { return bool(client); }

bool fc::Client::connected(long timeout_ms) const {
  channel->WaitForConnected(Util::deadline(timeout_ms));
  return channel->GetState(true) == GRPC_CHANNEL_READY;
}

bool fc::Client::check(const grpc::Status &status) {
  if (status.ok())
    return true;

  using grpc::StatusCode;
  switch (status.error_code()) {
  case StatusCode::UNAVAILABLE:
    log_service_unavailable();
    break;
  default:
    LOG(llvl::error) << status.error_message() << ": "
                     << status.error_details();
  }

  return false;
}

void fc::Client::log_service_unavailable() {
  LOG(llvl::fatal) << "Unable to connect to service; " << log::fmt_bold
                   << "start with 'fancon service'" << log::fmt_reset;
}

void fc::Client::enumerate_directory(const path &dir, std::ostream &os,
                                     uint depth) {
  static std::set<path> searched_symlinks;
  for (const auto &e : fs::recursive_directory_iterator(dir)) {
    os << e;
    if (e.is_regular_file()) {
      const string contents = Util::read_line(e).value_or("unreadable");
      os << ": " << contents;
    }
    os << endl;

    // Recurse into only the most shallow symlinks
    if (e.is_symlink() && depth == 0) {
      if (const path p = fs::read_symlink(e); searched_symlinks.count(p) == 0) {
        searched_symlinks.emplace(p);
        enumerate_directory(e, os, depth + 1);
      }
    }
  }
}
