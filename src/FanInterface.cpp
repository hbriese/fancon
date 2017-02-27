#include "FanInterface.hpp"

using namespace fancon;

FanInterface::FanInterface(const UID &uid, const FanConfig &conf, bool dynamic)
    : points(conf.points), hwID(to_string(uid.hwID)), dynamic(dynamic) {
  FanPaths p(uid, uid.type);
  tested = p.tested();

  if (tested) {
    // Read values from fancon dir
    rpm_min = read<int>(p.rpm_min_pf, hwID, uid.type);
    rpm_max = read<int>(p.rpm_max_pf, hwID, uid.type);
    pwm_min = read<int>(p.pwm_min_pf, hwID, uid.type);
    pwm_start = read<int>(p.pwm_start_pf, hwID, uid.type);
    slope = read<double>(p.slope_pf, hwID, uid.type);

    long temp = 0;
    stop_t = ((temp = read<long>(p.stop_t_pf, hwID, uid.type)) > 0) ? temp : 3000;
  }

  verifyPoints(uid);
}

void FanInterface::verifyPoints(const UID &fanUID) {
  bool invalidPoints = false, untestedPoints = false;
  stringstream ignoring;

  auto checkError = [&invalidPoints, &ignoring](fancon::Point &p, int val, int min, int max) -> bool {
    if (val > max || ((val < min) & (val != 0))) {
      ignoring << ' ' << p;
      return (invalidPoints = true);
    }
    return false;
  };

  // check if points are invalid: value larger than maximum, or small than min and non-zero
  auto invalidBegIt =
      std::remove_if(points.begin(), points.end(), [this, &checkError, &untestedPoints, &ignoring](fancon::Point &p) {
        if (p.validPWM())   // use pwm
          return checkError(p, p.pwm, pwm_min, pwm_max_absolute);
        else if (!tested) { // check if RPM can be used, else log and remove
          ignoring << ' ' << p;
          return (untestedPoints = true);
        } else if (!checkError(p, p.rpm, rpm_min, rpm_max)) {   // use rpm
          // calculate pwm and ensure calcPWM is valid
          auto cpwm = calcPWM(p.rpm);
          if (cpwm > 255)
            cpwm = 255;   // larger than max, set max
          else if (cpwm < 0)
            cpwm = 0;  // smaller than min, set min
          p.pwm = cpwm;
        }

        return false;
      });

  // erase invalid points
  points.erase(invalidBegIt, points.end());

  if (!ignoring.str().empty()) {
    stringstream err;
    err << fanUID;
    string spaces(err.str().size(), ' ');

    if (untestedPoints) {
      err << " : has not been tested yet, so RPM speed cannot be used (only PWM)";
      log(LOG_NOTICE, err.str());
      err.str("");
      err << spaces;
    }

    if (invalidPoints) {
      err << " : invalid config entries;"
          << "rpm min=" << rpm_min << "(or 0), max=" << rpm_max
          << "; pwm min=" << pwm_min << "(or 0), max=" << pwm_max_absolute;
      log(LOG_ERR, err.str());
      err.str("");
      err << spaces;
    }

    err << " : ignoring" << ignoring.str();
    log(LOG_ERR, err.str());
  }
}

void FanInterface::update(int temp) {
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
    } else if (readRPM() == 0)
      pwm = pwm_start;
  }

  writePWM(pwm);
}

void FanInterface::writeTestResult(const UID &uid, const FanTestResult &r, DeviceType devType) {
  FanPaths p(uid);
  string hwID = to_string(uid.hwID);

  write<int>(p.pwm_min_pf, hwID, r.pwm_min, devType);
  write<int>(p.pwm_max_pf, hwID, r.pwm_max, devType);
  write<int>(p.rpm_min_pf, hwID, r.rpm_min, devType);
  write<int>(p.rpm_max_pf, hwID, r.rpm_max, devType);
  write<int>(p.pwm_start_pf, hwID, r.pwm_start, devType);
  write<double>(p.slope_pf, hwID, r.slope, devType);
  write<long>(p.stop_t_pf, hwID, r.stop_time, devType);
}

FanPaths::FanPaths(const UID &uid, DeviceType devType)
    : dev_type(devType) {
  const string devID = to_string(fancon::Util::getLastNum(uid.dev_name));
  hwmonID = to_string(uid.hwID);

  string pwm_pf = "pwm" + devID;
  string rpm_pf = "fan" + devID;
  pwm_min_pf = pwm_pf + "_min";
  pwm_max_pf = pwm_pf + "_max";
  rpm_min_pf = rpm_pf + "_min";
  rpm_max_pf = rpm_pf + "_max";
  slope_pf = pwm_pf + "_slope";
  stop_t_pf = pwm_pf + "_stop_time";
  pwm_start_pf = pwm_pf + "_start";

  if (dev_type == FAN) {
    enable_pf = pwm_pf + "_enable";
    rpm_pf.append("_input");
    rpm_p = fancon::Util::getPath(rpm_pf, hwmonID, dev_type, true);
    pwm_p = fancon::Util::getPath(pwm_pf, hwmonID, dev_type, true);
  }
}

bool FanPaths::tested() const {
  auto gp = [this](const string &pathPF) { return getPath(pathPF, hwmonID, dev_type); };
  string paths[]{gp(pwm_min_pf), gp(pwm_max_pf), gp(rpm_min_pf), gp(rpm_max_pf),
                 gp(slope_pf), gp(stop_t_pf)};

  // fail if any path doesn't exist
  for (auto &p : paths)
    if (!exists(p))
      return false;

  return true;
}

FanTestResult tests::runTests(function<int()> rPWM, function<void(int)> wPWM, function<int()> rRPM,
                              function<void(int)> wEnableMode, int manualEnableMode, int prevEnableMode) {
  wEnableMode(manualEnableMode);

  // set full speed, and read rpm
  auto rpmMax = tests::getMaxRPM(wPWM, rRPM);

  // fail if rpm isn't recorded by the driver, or unable to write PWM
  if (rpmMax <= 0) {  // fail
    wEnableMode(prevEnableMode); // restore driver control
    return FanTestResult();
  }

  // lower the pwm while the rpm remains at it's maximum
  auto pwmMax = tests::getMaxPWM(wPWM, rRPM, rpmMax);

  // measure time to stop the fan
  auto stopTime = tests::getStopTime(wPWM, rRPM);

  // slowly raise pwm until fan starts again
  auto pwmStart = tests::getPWMStart(rPWM, wPWM, rRPM);

  // set pwm as max, then lower until it stops
  std::pair<int, int> PWM_RPM_min = tests::getPWMRPMMin(wPWM, rRPM, pwmStart);
  auto pwmMin = PWM_RPM_min.first;
  auto rpmMin = PWM_RPM_min.second;

  // slope is the gradient between the minimum and (real) max rpm/pwm
  auto slope = static_cast<double>((rpmMax - rpmMin)) / (pwmMax - pwmMin);

  // restore driver control
  wEnableMode(prevEnableMode);

  return FanTestResult(rpmMin, rpmMax, pwmMin, pwmMax, pwmStart, stopTime, slope);
}

int tests::getMaxRPM(function<void(int)> wPWM, function<int()> rRPM) {
  wPWM(pwm_max_absolute); // set fan to full sleed
  sleep(speed_change_t * 2);
  int rpm_max = 0;
  for (int prevRPMMax = -1; rpm_max > prevRPMMax; sleep(speed_change_t * 2)) {
    prevRPMMax = rpm_max;
    rpm_max = rRPM();
  }
  return rpm_max;
}

int tests::getMaxPWM(function<void(int)> wPWM, function<int()> rRPM, const int rpm_max) {
  int pwm_max = pwm_max_absolute;
  while ((rpm_max - 5) <= rRPM()) {
    pwm_max -= 5;
    wPWM(pwm_max - 5);
    sleep(speed_change_t * 2);
  }

  return pwm_max;
}

long tests::getStopTime(function<void(int)> wPWM, function<int()> rRPM) {
  wPWM(0);  // turn off fan
  auto now = chrono::high_resolution_clock::now();
  while (rRPM() > 0);
  auto after = chrono::high_resolution_clock::now();
  long stop_t_ms = chrono::duration_cast<chrono::milliseconds>(after - now).count();

  return stop_t_ms;
}

int tests::getPWMStart(function<int()> rPWM, function<void(int)> wPWM, function<int()> rRPM) {
  int pwmStart = 0;
  while (rRPM() <= 0) {
    wPWM((pwmStart += 5));
    sleep(speed_change_t * 2);
  }

  if (rPWM() != pwmStart)
    log(LOG_DEBUG, "The starting PWM has changed since writing it!");

  return pwmStart;
}

std::pair<int, int> tests::getPWMRPMMin(function<void(int)> wPWM, function<int()> rRPM, int startPWM) {
  int pwm_min(startPWM);
  wPWM(pwm_min);

  int rpm_min = 0, rmt;
  while ((rmt = rRPM()) > 0) {
    if (rmt < rpm_min)    // fans may increase their rpm
      break;
    rpm_min = rmt;
    pwm_min -= 5;
    wPWM(pwm_min - 5);
    sleep(speed_change_t * 2);
  }

  return std::make_pair(pwm_min, rpm_min);
}
