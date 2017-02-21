#include "Fan.hpp"

using namespace fancon;

Fan::Fan(const UID &fan_uid, const Config &conf, bool dynamic)
    : hwID(to_string(fan_uid.hwmon_id)), points(conf.points), dynamic(dynamic) {
  // makes all related fan paths, deleted after constructor
  FanPaths p(fan_uid);

  // keep paths (with changing values) for future rw
  pwm_p = p.pwm_p;
  rpm_p = p.rpm_p;
  enable_pf = p.enable_pf;

  // Read constant values from sysfs
  rpm_min = read<int>(p.rpm_min_pf, hwID);
  rpm_max = read<int>(p.rpm_max_pf, hwID);
  pwm_min = read<int>(p.pwm_min_pf, hwID);
  pwm_start = read<int>(p.pwm_start_pf, hwID);
  slope = read<double>(p.slope_pf, hwID);

  // read pwm enable mode set from the driver, to restore in fan deconstructor
  driver_enable_mode = read<int>(p.enable_pf, hwID);

  // set pwmX_enable to manual mode ('1') & start fan
  write<int>(p.enable_pf, hwID, manual_enable_mode);
  write<int>(p.pwm_p, pwm_start);

  // Set values to a sane default if they are not set; in ms
  // TODO: implement step_ut & step_dt usage
//    step_up_t = ((temp = read<long>(p.step_ut_pf, hwID)) > 0) ? temp : 2000;
//    step_down_t = ((temp = read<long>(p.step_dt_pf, hwID)) > 0) ? temp : 1000;
  long temp = 0;
  stop_t = ((temp = read<long>(p.stop_t_pf, hwID)) > 0) ? temp : 3000;

  // check config is valid
  auto checkError = [this, &fan_uid](vector<fancon::Point>::iterator &it, int val, int min, int max) {
    if (val > max || ((val < min) & (val != 0))) {
      std::stringstream err;
      err << "Invalid config entry: " << fan_uid << " " << *it
          << " - min=" << to_string(min) << "(or 0), max=" << to_string(max);
      log(LOG_ERR, err.str());
      points.erase(it);
      return true;
    }
    return false;
  };

  // check if points are invalid: value larger than maximum, or small than min and non-zero
  for (auto it = points.begin(); it != points.end(); ++it) {
    if (it->validPWM())  // use pwm
      checkError(it, it->pwm, pwm_min, pwm_max_absolute);
    else if (!checkError(it, it->rpm, rpm_min, rpm_max)) {            // use rpm
      // calculate pwm and ensure calcPWM is valid
      auto cpwm = calcPWM(it->rpm);
      if (cpwm > 255)
        cpwm = 255;   // larger than max, set max
      else if (cpwm < 0)
        cpwm = 0;  // smaller than min, set min
      it->pwm = cpwm;
    }
  }
}

Fan::~Fan() {
  // restore driver enable mode so it takes over fan control
  write<int>(enable_pf, hwID, driver_enable_mode);
}

void Fan::update(int temp) {
  // TODO: handle step_ut & step_dt

  auto it = std::lower_bound(points.begin(), points.end(), temp,
                             [](const fancon::Point &p1, const fancon::Point &p2) { return p1.temp < p2.temp; });

  // value is one less than or equal to temp, i.e. value before lower_bound
  if (it != points.begin())
    it = prev(it);

  if (it == points.end())
    it = prev(points.end());

  int pwm = it->pwm;
  auto nextIt = std::next(it);

  if (pwm > 0) {
    if (dynamic && nextIt != points.end()) {
      int nextPWM = (nextIt != points.end()) ? nextIt->pwm : calcPWM(nextIt->rpm);
      pwm = pwm + ((nextPWM - pwm) / (nextIt->temp - it->temp));
    } else if (read<int>(rpm_p, 0) == 0)
      pwm = pwm_start;
  }

  write<int>(pwm_p, pwm);
}

int Fan::testPWM(int rpm) {
  // assume the calculation is the real pwm for a starting point
  int nextPWM(calcPWM(rpm));

  write(pwm_p, nextPWM);
  sleep(speed_change_t * 3);

  int curPWM = 0, lastPWM = -1, curRPM;
  // break when RPM found when matching, or if between the 5 PWM margin
  while ((curRPM = read<int>(rpm_p, 0)) != rpm && nextPWM != lastPWM) {
    lastPWM = curPWM;
    curPWM = nextPWM;
    nextPWM = (curRPM < rpm) ? nextPWM + 2 : nextPWM - 2;
    if (nextPWM < 0)
      nextPWM = calcPWM(rpm);
    if (curRPM <= 0)
      nextPWM = pwm_start;

    write(pwm_p, nextPWM);
    sleep(speed_change_t);
  }

  return nextPWM;  // TODO: or next/last PWM??
}

TestResult Fan::test(const UID &fanUID, const bool profile) {
  FanPaths p(fanUID);
  string hwID = to_string(fanUID.hwmon_id);

  std::ifstream ifs;
  std::ofstream ofs;

  auto readPWM = [&p]() { return read<int>(p.pwm_p); };
  auto writePWM = [&p](int pwm) { return write<int>(p.pwm_p, pwm); };
  auto readRPM = [&p]() { return read<int>(p.rpm_p); };
  auto writeEnable = [&p, &hwID](int mode) { return write<int>(p.enable_pf, hwID, mode, true); };
  auto readEnable = [&p, &hwID]() { return read<int>(p.enable_pf, hwID, true); };

  auto getMaxRPM = [&fanUID, &writePWM, &readRPM]() {
    writePWM(pwm_max_absolute); // set fan to full sleed
    sleep(speed_change_t * 2);
    int rpm_max = 0;
    for (int prevRPMMax = -1; rpm_max > prevRPMMax; sleep(speed_change_t * 2)) {
      prevRPMMax = rpm_max;
      rpm_max = readRPM();
    }
    return rpm_max;
  };

  auto getMaxPWM = [&fanUID, &writePWM, &readRPM](const int rpm_max) {
    int pwm_max = pwm_max_absolute;
    while ((rpm_max - 5) <= readRPM()) {
      pwm_max -= 5;
      writePWM(pwm_max - 5);
      sleep(speed_change_t * 2);
    }

    return pwm_max;
  };

  auto getStopTime = [&writePWM, &readRPM]() {
    writePWM(0);  // set PWM to 0
    auto now = chrono::high_resolution_clock::now();
    while (readRPM() > 0);
    auto after = chrono::high_resolution_clock::now();
    long stop_t_ms = chrono::duration_cast<chrono::milliseconds>(after - now).count();

    return stop_t_ms;
  };

  auto getPWMStart = [&fanUID, &writePWM, &readRPM]() {
    int pwm_start = 0;
    while (readRPM() <= 0) {
      pwm_start += 5;
      writePWM(pwm_start);
      sleep(speed_change_t);
    }

    return pwm_start;
  };

  auto getPWMRPMMin = [&fanUID, &readPWM, &writePWM, &readRPM]() {
    // increase pwm
    int pwm_min = readPWM();
    if ((pwm_min + 20) < pwm_max_absolute)
      pwm_min += 20;
    writePWM(pwm_min);

    int rpm_min = 0, rmt;
    while ((rmt = readRPM()) > 0) {
      rpm_min = rmt;
      pwm_min -= 5;
      writePWM(pwm_min - 5);
      sleep(speed_change_t * 2);
    }

    return std::make_pair(pwm_min, rpm_min);
  };

  auto profiler = [&fanUID, &hwID, &writePWM, &readRPM]() {
    string profile_pf = fanUID.dev_name + "_profile";
    profile_pf = fancon::Util::getPath(profile_pf, hwID);
    ofstream pofs(profile_pf);
    pofs << "PWM RPM" << endl;
    // record rpm from PWM 255 to 0
    writePWM(255);
    sleep(speed_change_t * 3);
    for (int rpm, pwm = 255; pwm > 0; pwm -= 5) {
      writePWM(pwm);
      sleep(speed_change_t);
      rpm = readRPM();
      pofs << pwm << " " << rpm << endl;
    }
    if (pofs.fail()) {
      string err = string("Profiler failed for: ") + profile_pf;
      log(LOG_ERR, err);
    }
  };

  // store pre-test (likely driver controlled) fan mode, and set to manual
  int prev_mode = readEnable();
  int prev_pwm = readPWM();
  writeEnable(manual_enable_mode);

  // set full speed, and read rpm
  int rpm_max = getMaxRPM();

  // fail if rpm isn't recorded by the driver
  if (rpm_max <= 0) {  // fail
    writePWM(prev_pwm);
    writeEnable(prev_mode); // restore driver control
    return TestResult();
  }

  // lower the pwm while the rpm remains at it's maximum
  int pwm_max = getMaxPWM(rpm_max);

  // measure time to stop the fan
  long stop_time = getStopTime();

  // slowly raise pwm until fan starts again
  int pwm_start = getPWMStart();

  // set pwm as max, then lower until it stops
  std::pair<int, int> PWM_RPM_min = getPWMRPMMin();
  int pwm_min = PWM_RPM_min.first;
  int rpm_min = PWM_RPM_min.second;

  if (profile)
    profiler();

  // slope is the gradient between the minimum and (real) max rpm/pwm
  double slope = (double) (rpm_max - rpm_min) / (pwm_max - pwm_min);

  // restore driver control
  writePWM(prev_pwm);
  writeEnable(prev_mode);

  return TestResult(rpm_min, rpm_max, pwm_min, pwm_max, pwm_start, stop_time, slope);
}

void Fan::writeTestResult(const UID &fanUID, const TestResult &r) {
  FanPaths p(fanUID);
  string hwID = to_string(fanUID.hwmon_id);

  write<int>(p.pwm_min_pf, hwID, r.pwm_min);
  write<int>(p.pwm_max_pf, hwID, r.pwm_max);
  write<int>(p.rpm_min_pf, hwID, r.rpm_min);
  write<int>(p.rpm_max_pf, hwID, r.rpm_max);
  write<int>(p.pwm_start_pf, hwID, r.pwm_start);
  write<double>(p.slope_pf, hwID, r.slope);
  write<long>(p.stop_t_pf, hwID, r.stop_time);
}

void Fan::sleep(int s) { std::this_thread::sleep_for(chrono::seconds(s)); }

FanPaths::FanPaths(const UID &fanUID) {
  const string devID = to_string(fancon::Util::getLastNum(fanUID.dev_name));
  const string hwmonID = to_string(fanUID.hwmon_id);

  string pwm_pf = "pwm" + devID;
  string rpm_pf = "fan" + devID;
  enable_pf = pwm_pf + "_enable";
  pwm_min_pf = pwm_pf + "_min";
  pwm_max_pf = pwm_pf + "_max";
  rpm_min_pf = rpm_pf + "_min";
  rpm_max_pf = rpm_pf + "_max";
  slope_pf = pwm_pf + "_slope";
//  step_ut_pf = pwm_pf + "_step_up_time";
//  step_dt_pf = pwm_pf + "_step_down_time";
  stop_t_pf = pwm_pf + "_stop_time";
  rpm_pf.append("_input");
  rpm_p = fancon::Util::getPath(rpm_pf, hwmonID, true);
  pwm_p = fancon::Util::getPath(pwm_pf, hwmonID, true);

//    const vector<string> psPfs{pwm_pf + "_start", pwm_pf + "_auto_start"};
  // use '_start' by default, '_auto_start' if it exists
//    pwm_start_pf = (exists(Util::getPath(psPfs[1], hwmonID, 0))) ? psPfs[1] : psPfs[0];
  pwm_start_pf = pwm_pf + "_start";
}