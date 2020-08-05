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
    status();
  } else if (args.enable) {
    if (args.enable.has_value())
      enable(args.enable.value);
    else
      enable();
  } else if (args.disable) {
    if (args.disable.has_value())
      disable(args.disable.value);
    else
      disable();
  } else if (args.test) {
    if (args.test.has_value())
      test(args.test.value, true);
    else
      test(args.force);
  } else if (args.reload) {
    reload();
  } else if (args.stop_service) {
    stop_service();
  } else if (args.nv_init) {
    nv_init();
  } else if (args.sysinfo) {
    sysinfo(args.sysinfo.value);
  } else if (Util::is_atty() && !exists(args.config.value)) {
    // Offer test
    cout << log::fmt_green << "Test devices & generate a config? (y/n): ";
    char answer;
    std::cin >> answer;
    if (answer == 'y') {
      test(args.force);
    }
  } else { // else, excluding help which has already run
    print_help(args.config.value);
  }
}

void fc::Client::stop_service() {
  ClientContext context;
  if (check(client->StopService(&context, empty, &empty)))
    LOG(llvl::info) << "Service stopped";
  else
    LOG(llvl::error) << "Failed to stop service";
}

optional<fc_pb::Devices> fc::Client::get_devices() {
  ClientContext context;
  fc_pb::Devices devices;
  if (!check(client->GetDevices(&context, empty, &devices)))
    return nullopt;

  return devices;
}

optional<fc_pb::Devices> fc::Client::get_enumerated_devices() {
  ClientContext context;
  fc_pb::Devices devices;
  if (!check(client->GetEnumeratedDevices(&context, empty, &devices)))
    return nullopt;

  return devices;
}

void fc::Client::status() {
  const auto devices = get_devices();
  if (!devices || devices->fan_size() == 0) {
    LOG(llvl::info) << "No devices found";
    return;
  }

  // Find the status of all devices
  size_t longest_label = 0;
  vector<fc_pb::FanStatus> statuses;
  for (const auto &f : devices->fan()) {
    ClientContext context;
    fc_pb::FanStatus status;
    if (check(client->GetFanStatus(&context, from(f.label()), &status))) {
      if (const auto l = status.label().length(); l > longest_label)
        longest_label = l;
      statuses.push_back(move(status));
    }
  }

  // Write out all the collected status, width adjusting the outputs
  for (const auto &s : statuses) {
    stringstream extras;
    if (s.status() != FanStatus::FanStatus_Status_DISABLED)
      extras << " " << setw(5) << s.rpm() << "rpm " << setw(3) << s.pwm()
             << "pwm";

    cout << setw(longest_label) << s.label() << ": " << setw(8)
         << status_text(s.status()) << extras.rdbuf() << endl;
  }
}

void fc::Client::enable(const string &flabel) {
  ClientContext context;
  if (check(client->Enable(&context, from(flabel), &empty)))
    LOG(llvl::info) << flabel << ": enabled";
}

void fc::Client::enable() {
  ClientContext context;
  if (check(client->EnableAll(&context, empty, &empty)))
    status();
}

void fc::Client::disable(const string &flabel) {
  ClientContext context;
  if (check(client->Disable(&context, from(flabel), &empty)))
    LOG(llvl::info) << flabel << ": disabled";
}

void fc::Client::disable() {
  ClientContext context;
  if (check(client->DisableAll(&context, empty, &empty)))
    status();
}

void fc::Client::test(bool forced) {
  const auto devices = get_devices();
  if (!devices || devices->fan_size() == 0) {
    LOG(llvl::info) << "No devices found";
    return;
  }

  mutex write_mutex;
  vector<thread> threads;
  for (const auto &f : devices->fan()) {
    const string &flabel = f.label();
    threads.emplace_back([&, flabel] {
      ClientContext context;
      fc_pb::TestRequest req;
      req.set_device_label(flabel);
      req.set_forced(forced);

      auto reader = client->Test(&context, req);
      fc_pb::TestResponse resp;
      while (reader->Read(&resp)) {
        const lock_guard<mutex> lock(write_mutex);
        LOG(llvl::info) << flabel << ": " << resp.status() << "%";
      }
      if (!check(reader->Finish()))
        LOG(llvl::error) << flabel << ": test failed";
    });
  }

  for (auto &t : threads) {
    if (t.joinable())
      t.join();
  }
}

void fc::Client::test(const string &flabel, bool forced) {
  ClientContext context;
  fc_pb::TestRequest req;
  req.set_device_label(flabel);
  req.set_forced(forced);

  auto reader = client->Test(&context, req);
  fc_pb::TestResponse resp;
  while (reader->Read(&resp)) {
    LOG(llvl::info) << flabel << ": " << resp.status() << "%";
  }

  if (!check(reader->Finish()))
    LOG(llvl::error) << flabel << ": test failed";
}

void fc::Client::reload() {
  ClientContext context;
  if (check(client->Reload(&context, empty, &empty)))
    LOG(llvl::info) << "Reloaded";
  else
    LOG(llvl::error) << "Failed to reload";
}

void fc::Client::nv_init() {
  ClientContext context;
  if (!check(client->NvInit(&context, empty, &empty)))
    LOG(llvl::error) << "Failed to init nvidia";
}

void fc::Client::sysinfo(const string &p) {
  string out;
  std::ofstream ofs(p);

  ClientContext context;
  ofs << "Enumerated:" << endl;
  fc_pb::Devices enumerated;
  if (check(client->GetEnumeratedDevices(&context, empty, &enumerated))) {
    google::protobuf::TextFormat::PrintToString(enumerated, &out);
    ofs << out;
  } else {
    ofs << "Failed";
  }

  ClientContext context_user;
  ofs << endl << "User:" << endl;
  fc_pb::Devices user;
  if (check(client->GetDevices(&context_user, empty, &user))) {
    google::protobuf::TextFormat::PrintToString(user, &out);
    ofs << out;
  } else {
    ofs << "Failed";
  }

  const path hwmon_dir = "/sys/class/hwmon";
  ofs << endl << hwmon_dir.string() << ":" << endl;
  enumerate_directory(hwmon_dir, ofs);

  // TODO: logs

  if (ofs) {
    // Allow all users to read & write to the file
    const auto perms = fs::perms::others_read | fs::perms::others_write;
    fs::permissions(p, perms, fs::perm_options::add);

    LOG(llvl::info) << "Sysinfo written to: " << p;
  } else {
    LOG(llvl::error) << "Failed to write sysinfo to: " << p;
  }
}

void fc::Client::print_help(const string &conf) {
  LOG(llvl::info) << "fancon arg [value] ..." << endl
                  << "h  help           Show this help" << endl
                  << "s  status         Status of all fans" << endl
                  << "e  enable         Enable control of all fans" << endl
                  << "e  enable  [fan]  Enable control of the fan" << endl
                  << "d  disable        Disable control of all fans" << endl
                  << "d  disable [fan]  Disable control of the fans" << endl
                  << "t  test           Test all (untested) fans" << endl
                  << "t  test    [fan]  Test the fan (forced)" << endl
                  << "f  force          Test even already tested fans "
                  << "(default: false)" << endl
                  << "r  reload         Reload config" << endl
                  << "c  config  [file] Config path (default: "
                  << log::fmt_green_bold << conf << log::fmt_reset << ")"
                  << endl
                  << "   service        Start as service" << endl
                  << "   daemon         Daemonize the process (default: false)"
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

  switch (status.error_code()) {
  case StatusCode::UNAVAILABLE:
    log_service_unavailable();
    break;
  case StatusCode::NOT_FOUND:
    LOG(llvl::error) << status.error_message() << ": not found";
    break;
  default:
    LOG(llvl::error) << status.error_message() << ": "
                     << status.error_details();
  }

  return false;
}

void fc::Client::log_service_unavailable() {
  LOG(llvl::fatal) << "Unable to connect to service; " << endl
                   << log::fmt_bold << "start with 'sudo fancon service'"
                   << log::fmt_reset;
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

string fc::Client::status_text(fc_pb::FanStatus_Status status) {
  switch (status) {
  case fc_pb::FanStatus_Status_ENABLED:
    return "enabled";
  case fc_pb::FanStatus_Status_DISABLED:
    return "disabled";
  case fc_pb::FanStatus_Status_TESTING:
    return "testing";
  default:
    return "invalid";
  }
}

fc_pb::FanLabel fc::Client::from(const string &flabel) {
  fc_pb::FanLabel l;
  l.set_label(flabel);
  return l;
}
