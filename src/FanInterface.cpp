#include "FanInterface.hpp"

using namespace fancon;

FanInterface::FanInterface(const UID &uid, const FanConfig &c, bool dynamic, int driverEnableM, int manualEnableM)
    : points(c.points), driver_enable_mode(driverEnableM), manual_enable_mode(manualEnableM), hwID(uid.hwID),
      hwIDStr(to_string(hwID)), dynamic(dynamic) {
  FanPaths p(uid, uid.type);
  tested = p.tested();

  if (tested) {
    // Read values from fancon dir
    rpm_min = read<int>(p.rpm_min_pf, hwIDStr, uid.type);
    rpm_max = read<int>(p.rpm_max_pf, hwIDStr, uid.type);
    pwm_min = read<int>(p.pwm_min_pf, hwIDStr, uid.type);
    pwm_start = read<int>(p.pwm_start_pf, hwIDStr, uid.type);
    slope = read<double>(p.slope_pf, hwIDStr, uid.type);

    long temp = 0;
    stop_t = ((temp = read<long>(p.stop_t_pf, hwIDStr, uid.type)) > 0) ? temp : 3000;
  }

  verifyPoints(uid);
}

void FanInterface::verifyPoints(const UID &fanUID) {
  bool invalidPoints = false, untestedPoints = false;
  stringstream ignoring;

  auto hasError = [&invalidPoints, &ignoring](fancon::Point &p, int val, int min, int max) -> bool {
    if (val > max || ((val < min) & (val != 0))) {
      ignoring << ' ' << p;
      return (invalidPoints = true);
    }
    return false;
  };

  // check if points are invalid: value larger than maximum, or small than min and non-zero
  auto invalidBegIt =
      std::remove_if(points.begin(), points.end(), [this, &hasError, &untestedPoints, &ignoring](fancon::Point &p) {
        bool rm = true;

        if (p.validPWM())   // use pwm
          rm = hasError(p, p.pwm, pwm_min, pwm_max_absolute);
        else if (!tested) { // check if RPM can be used, else log and remove
          ignoring << ' ' << p;
          rm = (untestedPoints = true);
        } else if (!(rm = hasError(p, p.rpm, rpm_min, rpm_max))) {   // use rpm
          // calculate pwm and ensure calcPWM is valid
          auto cpwm = calcPWM(p.rpm);
          if (cpwm > 255)
            cpwm = 255;   // larger than max, set max
          else if (cpwm < 0)
            cpwm = 0;  // smaller than min, set min
          p.pwm = cpwm;
        }

        return rm;
      });

  // erase invalid points
  if (invalidBegIt != points.end())
    points.erase(invalidBegIt, points.end());

  if (!ignoring.str().empty()) {
    stringstream uss;
    uss << fanUID;
    string spaces(uss.str().size(), ' ');

    if (untestedPoints)
      LOG(llvl::warning) << uss.str() << " : has not been tested yet, so RPM speed cannot be used (only PWM)";

    if (invalidPoints)
      LOG(llvl::warning) << ((untestedPoints) ? spaces : uss.str()) << " : invalid config entries;"
                         << "rpm min=" << rpm_min << "(or 0), max=" << rpm_max
                         << "; pwm min=" << pwm_min << "(or 0), max=" << pwm_max_absolute;

    LOG(llvl::warning) << spaces << " : ignoring" << ignoring.str();
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
  auto nextIt = next(it);

  if (pwm > 0) {
    if (stopped)  // start fan if stopped
      pwm = pwm_start;
    else if (dynamic && nextIt != points.end())   // interpolate PWM between present and next point
      pwm = pwm + ((nextIt->pwm - pwm) / (nextIt->temp - it->temp));
  } else
    stopped = true;

  writePWM(pwm);
}

FanTestResult FanInterface::test() {
  writeEnableMode(manual_enable_mode);

  // pre-test RPM
  auto ptRPM = readRPM();

  // set full speed, and read rpm
  auto rpmMax = getMaxRPM();

  // fail if rpm isn't recorded by the driver, or unable to write PWM
  if (rpmMax <= 0) {  // fail
    writeEnableMode(driver_enable_mode); // restore driver control
    return FanTestResult();
  }

  // lower the pwm while the rpm remains at it's maximum
  auto pwmMax = getMaxPWM(rpmMax);

  // measure time to stop the fan
  auto stopTime = getStopTime();

  // slowly raise pwm until fan starts again
  auto pwmStart = getPWMStart();

  // set pwm as max, then lower until it stops
  std::pair<int, int> PWM_RPM_min = getPWMRPMMin(pwmStart);
  auto pwmMin = PWM_RPM_min.first;
  auto rpmMin = PWM_RPM_min.second;

  // slope is the gradient between the minimum and (real) max rpm/pwm
  auto slope = static_cast<double>((rpmMax - rpmMin)) / (pwmMax - pwmMin);

  // restore pre-test RPM & driver control
  writePWM(ptRPM);
  writeEnableMode(driver_enable_mode);

  return FanTestResult(rpmMin, rpmMax, pwmMin, pwmMax, pwmStart, stopTime, slope);
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
  hwID = to_string(uid.hwID);

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
    rpm_p = fancon::Util::getPath(rpm_pf, hwID, dev_type, true);
    pwm_p = fancon::Util::getPath(pwm_pf, hwID, dev_type, true);
  }
}

bool FanPaths::tested() const {
  auto gp = [this](const string &pathPF) { return getPath(pathPF, hwID, dev_type); };
  string paths[]{gp(pwm_min_pf), gp(pwm_max_pf), gp(rpm_min_pf), gp(rpm_max_pf),
                 gp(slope_pf), gp(stop_t_pf)};

  // fail if any path doesn't exist
  for (auto &p : paths)
    if (!exists(p))
      return false;

  return true;
}

int FanInterface::getMaxRPM() {
  writePWM(pwm_max_absolute); // set fan to full sleed
  sleep(speed_change_t * 2);
  int rpm_max = 0;
  for (int prevRPMMax = -1; rpm_max > prevRPMMax; sleep(speed_change_t * 2)) {
    prevRPMMax = rpm_max;
    rpm_max = readRPM();
  }
  return rpm_max;
}

int FanInterface::getMaxPWM(const int rpm_max) {
  int pwm_max = pwm_max_absolute;
  while ((rpm_max - 5) <= readRPM()) {
    pwm_max -= 5;
    writePWM(pwm_max - 5);
    sleep(speed_change_t * 2);
  }

  return pwm_max;
}

long FanInterface::getStopTime() {
  writePWM(0);  // turn off fan
  auto now = chrono::high_resolution_clock::now();
  while (readRPM() > 0);
  auto after = chrono::high_resolution_clock::now();
  long stop_t_ms = chrono::duration_cast<chrono::milliseconds>(after - now).count();

  return stop_t_ms;
}

int FanInterface::getPWMStart() {
  int pwmStart = 0;
  while (readRPM() <= 0) {
    writePWM((pwmStart += 5));
    sleep(speed_change_t * 2);
  }

  if (readPWM() != pwmStart)  // pwm has changed since writing it to device
    LOG(llvl::debug) << "PWM control is not exclusive - this may cause inaccurate testing results";

  return pwmStart;
}

std::pair<int, int> FanInterface::getPWMRPMMin(const int startPWM) {
  int pwm_min(startPWM);
  writePWM(pwm_min);

  int rpm_min = 0, rmt;
  while ((rmt = readRPM()) > 0) {
    if (rmt < rpm_min)    // fans may increase their rpm
      break;
    rpm_min = rmt;
    pwm_min -= 5;
    writePWM(pwm_min - 5);
    sleep(speed_change_t * 2);
  }

  return std::make_pair(pwm_min, rpm_min);
}
