#include "main.hpp"

string fancon::help() {
  stringstream ss;
  ss << "fancon usage:" << endl
     << "<Argument>     <Parameters>  <Details>" << endl
     << "lf list-fans                 Lists the UIDs of all fans" << endl
     << "ls list-sensors              List the UIDs of all temperature sensors" << endl
     << "test           profile       Tests the fan characteristic of all fans, required for usage of RPM in "
     << fancon::conf_path << endl
     << "wc write-conf                Writes missing fan UIDs to " << fancon::conf_path << endl
     << "start                        Starts the fancon daemon" << endl
     << "stop                         Stops the fancon daemon" << endl
     << "reload                       Reloads the fancon daemon" << endl;

  return ss.str();
}

void fancon::FetchTemp() {
  sensors_chip_name const *cn = nullptr;
  int c = 0;
  while ((cn = sensors_get_detected_chips(NULL, &c)) != NULL) {
    std::cout << "Chip: " << cn->prefix << "/" << cn->path << std::endl;

    sensors_feature const *feat;
    int f = 0;

    while ((feat = sensors_get_features(cn, &f)) != 0) {
      std::cout << f << ": " << feat->name << std::endl;

      sensors_subfeature const *subf;
      int s = 0;

      /*while ((subf = sensors_get_subfeature(cn, feat, SENSORS_SUBFEATURE_TEMP_INPUT)) != NULL) {
          cout << "sf: " << subf->name << "/" << subf->number << " " << subf->type << " = ";
          double v;
          sensors_get_value(cn, subf->number, &v);
          cout << v << endl;
      } */

      while ((subf = sensors_get_all_subfeatures(cn, feat, &s)) != NULL) {
        std::cout << f << ":" << s << ":" << subf->name
                  << "/" << subf->number << " = ";

        double val;
        if (subf->flags & SENSORS_MODE_R) {
          int rc = sensors_get_value(cn, subf->number, &val);
          if (rc < 0) {
            std::cout << "err: " << rc;
          } else {
            std::cout << val;
          }
        }
        if (subf->flags & SENSORS_MODE_W)
          cout << " WRITABLE";

        std::cout << std::endl;
      }
    }
  }
}

string fancon::listFans(SensorController &sc) {
  auto uids = sc.getUIDs(Fan::path_pf);
  stringstream ss;

  for (auto &uid : uids)
    ss << uid << endl;

  return ss.str();
}

string fancon::listSensors(SensorController &sc) {
  auto uids = sc.getUIDs(TemperatureSensor::path_pf);
  stringstream ss;

  // add fan labels after UID
  for (auto &uid : uids) {
    ss << uid;

    string label_p(uid.getBasePath());
    string label = fancon::Util::readLine(label_p.append("_label"));
    if (!label.empty())
      ss << " (" << label << ")";

    ss << endl;
  }

  return ss.str();
}

void fancon::writeLock() {
  pid_t pid = fancon::Util::read<pid_t>(fancon::pid_file);
  string pid_path = string("/proc/") + to_string(pid);
  if (exists(pid_path)) {  // fancon process running
    std::cerr << "Error: a fancon process is already running, please close it to continue" << endl;
    exit(1);
  } else
    write<pid_t>(fancon::pid_file, getpid());
}

void fancon::test(SensorController &sc, const bool profileFans) {
  fancon::writeLock();

  auto f_uids = sc.getUIDs(fancon::Fan::path_pf);
  if (!exists(Util::fancon_dir))
    bfs::create_directory(Util::fancon_dir);

  vector<thread> threads;
  cout << "Starting tests" << endl;
  for (auto it = f_uids.begin(); it != f_uids.end(); ++it) {
    if (it->hwmon_id != std::prev(it)->hwmon_id)
      bfs::create_directory(string(fancon::Util::fancon_path) + to_string(it->hwmon_id));

    threads.push_back(thread(&fancon::testUID, std::ref(*it), profileFans));
  }

  // rejoin threads
  for (auto it = threads.begin(); it != threads.end(); ++it) {
    if (it->joinable())
      it->join();
    else {  // log uid from which the unjoinable thread was spawned
      stringstream err;
      err << "Failed to join thread for UID: " << *(f_uids.begin() + std::distance(threads.begin(), it));
      log(LOG_DEBUG, err.str().c_str());
      it->detach();
    }
  }
}
void fancon::testUID(UID &uid, const bool profileFans) {
  std::stringstream ss;
  int retries = 4;
  for (; retries > 0; --retries) {
    auto res = Fan::test(uid, profileFans);

    if (res.valid()) {
      Fan::writeTestResult(uid, res);
      ss << "Test passed for: " << uid << endl;
      break;
    } else if (!res.fail())
      res = Fan::test(uid, profileFans);
    else {
      ss << "Test failed for: " << uid << endl;
      break;
    }
  }

  if (retries <= 0)
    ss << "Test failed for: " << uid << endl;

  coutThreadsafe(ss.str());
  log(LOG_DEBUG, ss.str());
  ss.str("");
}

void fancon::start(const bool debug) {
  fancon::writeLock();

  // Fork off the parent process
  pid_t pid = fork();
  if (!debug) {
    if (pid > 0)
      exit(EXIT_SUCCESS);
    else if (pid < 0) {
      log(LOG_ERR, "Failed to fork off while starting fancond. An extra thread is running!");
      exit(EXIT_FAILURE);
    }
  }

  // Create a new session for the child
  if ((pid = setsid()) < 0) {
    if (!debug) {
      log(LOG_ERR, "Failed to set session id for the forked fancond thread");
      exit(EXIT_FAILURE);
    }
  }

  // update pid file
  write<pid_t>(fancon::pid_file, pid);

  // Ignore signal sent from child to parent process
  signal(SIGCHLD, SIG_IGN);

  // Set file mode
  umask(0);

  // Change working directory
  if (chdir("/") < 0) {
    // log failure
    exit(EXIT_FAILURE);
  }

  // close all file descriptors   // TODO: just close standard ones?
//    for (int fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--)
//        close(fd);
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // TODO: re-open stdin, stdout & stderr?

  SensorController sc;
  auto ts_parents = sc.readConf(fancon::conf_path);
  log(LOG_NOTICE, "Fancond started");

  // TODO: multi threaded
  /*
  vector<thread> threads;
  auto nThreads = std::thread::hardware_concurrency();
  auto nTSPs = std::distance(ts_parents.begin(), ts_parents.end());
  auto remainingThreads = nTSPs % nThreads;
  int tspsPerThread[nThreads] {(int) std::floor(nTSPs / nThreads)};

//    for (unsigned int index0 = )

  for (int i = 0; remainingThreads; ++i) {
      if (i >= nThreads)
          i = 0;
      tspsPerThread[i] += 1;
  }

  const unsigned int nTasks = nTSPs / nThreads,
      nTougherTaks = nTSPs % nThreads;
  for( unsigned int index0 = (thid < numTougherThreads ? thid * (numTasks+1) : NUMELEM - (THREADCNT - thid) * numTasks),
           index = index0; index < index0 + numTasks + (thid < numTougherThreads) ; ++index)
           */

  vector<thread> threads;
  DaemonMessage message(DaemonMessage::RUN);
  if (!ts_parents.empty())
    threads.push_back(thread(
        &SensorController::run, std::ref(sc), ts_parents.begin(), ts_parents.end(), std::ref(message)
    ));

  // remove endpoint if already present
  unlink(fancon::endpoint.path().c_str());
  basio::io_service ios;
  stream_protocol::acceptor acceptor(ios, fancon::endpoint);
  stream_protocol::socket socket(ios);
  acceptor.accept(socket);

  basio::streambuf sb;
  boost::system::error_code ec;
  while (basio::read(socket, sb, ec)) {// threads read reference to message and stop
    string in(basio::buffers_begin(sb.data()), basio::buffers_end(sb.data()));
    message = (DaemonMessage) stoi(in);

    if (ec)
      log(LOG_ERR, ec.message());
  }
  socket.close();

  // join threads
  for (auto &t: threads)
    if (t.joinable())
      t.join();
    else
      t.detach();

  // re-run this function if reload
  if (message == DaemonMessage::RELOAD)
    fancon::start(debug);
}

void fancon::send(DaemonMessage message) {
  basio::io_service ios;
  stream_protocol::socket socket(ios);
  boost::system::error_code ec;
  socket.connect(fancon::endpoint, ec);

  if (ec) {   // socket connection error
    std::cerr << "fancond is not running" << endl;
    log(LOG_DEBUG, string("Socket connect error ") + to_string(ec.value()) + ": " + ec.message());
    return;
  }

  string msg_str(to_string(message));
  basio::streambuf buf;
  ostream os(&buf);
  os.write(msg_str.data(), msg_str.size());
  socket.write_some(buf.data());
}

int main(int argc, char *argv[]) {
  vector<string> strArgv;
  for (int i = 1; i < argc; ++i)
    strArgv.push_back(string(argv[i]));

  // for first time setup
  fancon::Util::writeSyslogConf();

//    cout << "Hello, World!" << endl;
//    fancon::FetchTemp();
//    cout << endl;

  fancon::Arg help("help"), start("start"), stop("stop"), reload("reload"),
      list_fans("list-fans", true), list_sensors("list-sensors", true),
      test("test"), write_config("write-config");
  vector<reference_wrapper<fancon::Arg>> args
      {help, start, stop, reload, list_fans, list_sensors, test, write_config};

  fancon::Param debug("debug"), profiler("profiler"), exclude_fans("exclude-fans");
  vector<reference_wrapper<fancon::Param>> params
      {debug, profiler, exclude_fans};

  for (auto &a : strArgv) {
    if (a.empty())
      continue;

    // remove preceding '-'s
    while (a.front() == '-')
      a.erase(a.begin());

    // convert to lower case
    std::transform(a.begin(), a.end(), a.begin(), ::tolower);

    // set param or arg to called if equal
    for (auto &p : params)
      if (p.get() == a) {
        p.get().called = true;
        continue;
      }

    for (auto &arg : args)
      if (arg.get() == a)
        arg.get().called = true;
  }

  SensorController sc(debug.called);

  // execute called args with called params
  if (help.called || strArgv.empty()) // execute help() if no args are given
    coutThreadsafe(fancon::help());
  else if (start.called)
    fancon::start(debug.called);
  else if (stop.called)
    fancon::send(DaemonMessage::STOP);
  else if (reload.called)
    fancon::send(DaemonMessage::RELOAD);
  else if (list_fans.called)
    coutThreadsafe(fancon::listFans(sc));
  else if (list_sensors.called)
    coutThreadsafe(fancon::listSensors(sc));
  else if (test.called)
    fancon::test(sc, profiler.called);
  else if (write_config.called)
    sc.writeConf(fancon::conf_path);

  return 0;
}
/*static string generate_fan_uid2 (string &hwmon_sensor_path, const uint fan_id) {
    // get hwmon sensor id
    ulong id_pos = hwmon_sensor_path.find_last_of("hwmon", 0) +6;   // 6 = hwmon +1
    string hwmon_id = hwmon_sensor_path.substr(id_pos, hwmon_sensor_path.size());

    // remove appending '/'s
    while (*(hwmon_id.end()) == '/')
        hwmon_id.pop_back();

    // hwmon_id:fan_id
    return hwmon_id.append(":").append(std::to_string(fan_id));
} */