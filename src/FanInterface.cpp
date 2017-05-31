#include "FanInterface.hpp"

using namespace fancon;

FanInterface::FanInterface(const UID &uid, const fan::Config &conf, bool dyn,
                           enable_mode_t driverEnableM,
                           enable_mode_t manualEnableM)
    : points(conf.points), manual_enable_mode(manualEnableM), // TODO move conf
      driver_enable_mode(driverEnableM), hw_id(uid.hw_id),
      hw_id_str(to_string(hw_id)), dynamic(dyn) {
  FanCharacteristicPaths p(uid);
  const auto &tested = p.tested();

  FanCharacteristics c;
  if (tested) {
    // Read values from fancon dir
//    auto readIn = [this, &uid] (auto &data, const auto &pf) {
//      data = read<decltype(data)>(pf, hw_id_str, uid.type);
//    };

//    readIn(c.rpm_min, p.rpm_min_pf);
//    readIn(c.rpm_max, p.rpm_max_pf);
//    readIn(c.pwm_min, p.pwm_min_pf);
//    readIn(c.pwm_start, p.pwm_start_pf);
//    readIn(c.slope, p.slope_pf);

    c.rpm_min = read<decltype(c.rpm_min)>(p.rpm_min_pf, hw_id_str, uid.type);
    c.rpm_max = read<decltype(c.rpm_max)>(p.rpm_max_pf, hw_id_str, uid.type);
    c.pwm_min = read<decltype(c.pwm_min)>(p.pwm_min_pf, hw_id_str, uid.type);
    c.slope = read<decltype(c.slope)>(p.slope_pf, hw_id_str, uid.type);
    pwm_start =
        read<decltype(c.pwm_start)>(p.pwm_start_pf, hw_id_str, uid.type);
    wait_time = milliseconds(
        read<decltype(wait_time.count())>(p.wait_time_pf, hw_id_str, uid.type));
  }

  // Validate points, sort by temperature and shrink
  validatePoints(uid, tested, c);
  std::sort(points.begin(), points.end(),
            [](const Point &l, const Point &r) { return l.temp < r.temp; });
  prev_it = points.begin();
  points.shrink_to_fit();
}

/// \brief Attempts to re-enable manual enable mode
bool FanInterface::recoverControl(const string &deviceLabel) {
  for (auto i = 0; i < 3; ++i, sleep_for(wait_time))
    if (writeEnableMode(manual_enable_mode)) {
      LOG(llvl::trace) << "Regained control of: " << deviceLabel;
      return true;
    }

  LOG(llvl::warning) << "Lost control of: " << deviceLabel;
  return false;
}

void FanInterface::update(const temp_t temp) {
  auto comparePoints =
      [](const Point &p1, const Point &p2) { return p1.temp < p2.temp; };

  // Narrow search space by comparing to the previous iterator
  auto it = (temp < prev_it->temp)
            ? std::upper_bound(points.begin(), prev_it, temp, comparePoints)
            : std::upper_bound(prev_it, points.end(), temp, comparePoints);

  // std::upper_bound returns element greater-than
  // Therefore the previous element is less-or-equal (unless last point)
  if (it != points.begin())
    std::advance(it, -1);

  pwm_t newPwm{it->pwm};
  decltype(it) nextIt;

  if (newPwm > 0) {
    // Start fan if stopped, or calculate dynamic if not the highest element
    if (readRPM() == 0)
      newPwm = pwm_start;
    else if (dynamic && (nextIt = next(it)) != points.end())
      newPwm += ((nextIt->pwm - newPwm) / (nextIt->temp - it->temp));
  }

  writePWM(newPwm);
}

TestResult FanInterface::test() {
  // Store pre-test PWM to restore when test finished
  auto ptPWM = readPWM();

  // Fail early if can't write enable mode or PWM
  if (!writeEnableMode(manual_enable_mode))
    return {FanCharacteristics(), FailState::control};

  if (!writePWM(pwm_max_abs))
    return {FanCharacteristics(), FailState::pwm_write};

  // Assert state when assuming, and set after change
  FanState state = FanState::unknown;

  // Arbitrary baseline wait time for rpm change
  auto rpmMax = getMaxRPM(state, seconds(6));

  // Fail if RPM isn't being reported by the driver
  if (rpmMax <= 0)
    return {FanCharacteristics(), FailState::rpm_read_accuracy};

  // 99.5% of rpmMax
  auto closeRpmMax = static_cast<rpm_t>(rpmMax - (rpmMax / 20));

  const auto waitTime = getMaxSpeedChangeTime(state, closeRpmMax);
  auto pwmMax = getMaxPWM(state, waitTime, closeRpmMax);
  auto pwmStart = getPWMStart(state, waitTime);

  // Set pwm as max, then lower until it stops
  //  auto [pwmMin, rpmMin] = getMinAttributes(state, waitTime, pwmStart);
  //  TODO C++17
  pwm_t pwmMin;
  rpm_t rpmMin;
  std::tie(pwmMin, rpmMin) = getMinAttributes(state, waitTime, pwmStart);

  // Slope is the gradient between the minimum and (actual) max rpm/pwm
  auto slope = static_cast<slope_t>((rpmMax - rpmMin)) / (pwmMax - pwmMin);

  // Restore pre-test RPM & driver control
  writePWM(ptPWM);

  return {FanCharacteristics(rpmMin, rpmMax, pwmStart, pwmMin,
                             pwmMax, slope, waitTime), FailState::null};
}

void FanInterface::writeTestResult(const UID &uid, const FanCharacteristics &c,
                                   DeviceType devType) {
  FanCharacteristicPaths p(uid);
  string hwID = to_string(uid.hw_id);

  auto writeRes = [&hwID, &devType](const auto &pf, const auto &data) {
    write(pf, hwID, data, devType);
  };

  writeRes(p.rpm_min_pf, c.rpm_min);
  writeRes(p.rpm_max_pf, c.rpm_max);
  writeRes(p.pwm_min_pf, c.pwm_min);
  writeRes(p.pwm_max_pf, c.pwm_max);
  writeRes(p.pwm_start_pf, c.pwm_start);
  writeRes(p.slope_pf, c.slope);
  writeRes(p.wait_time_pf, c.wait_time.count());
}

FanCharacteristicPaths::FanCharacteristicPaths(const UID &uid)
    : type(uid.type), hw_id(to_string(uid.hw_id)) {
  const string devID(to_string(getDeviceID(uid)));

  string pwm_pf = pwm_prefix + devID;
  string rpm_pf = rpm_prefix + devID;
  pwm_min_pf = pwm_pf + "_min";
  pwm_max_pf = pwm_pf + "_max";
  rpm_min_pf = rpm_pf + "_min";
  rpm_max_pf = rpm_pf + "_max";
  slope_pf = pwm_pf + "_slope";
  pwm_start_pf = pwm_pf + "_start";
  wait_time_pf = pwm_pf + "_stop_time";
}

bool FanCharacteristicPaths::tested() const {
  // Fail if any path doesn't exist
  for (const auto &pf : {&pwm_min_pf, &pwm_max_pf, &rpm_min_pf, &rpm_max_pf,
                         &slope_pf, &wait_time_pf})
    if (!exists(getPath(*pf, hw_id, type)))
      return false;

  return true;
}

pwm_t FanInterface::calcPWM(const rpm_t &rpmMin, const pwm_t &pwmMin,
                            const slope_t &slope, const rpm_t &rpm) {
  auto cpwm = static_cast<pwm_t>(((rpm - rpmMin) / slope) + pwmMin);

  // Enforce PWM absolute bounds
  if (cpwm > pwm_max_abs)
    cpwm = pwm_max_abs;
  else if (cpwm < pwm_min_abs)
    cpwm = pwm_min_abs;

  return cpwm;
}

void FanInterface::validatePoints(const UID &fanUID, const bool &tested,
                                  const FanCharacteristics &c) {
  bool invalidPoints = false, untestedPoints = false;
  stringstream ignoring;

  /// \return True if <= max or >= min and non-zero
  auto withinBounds = [&](const fan::Point &p, const auto &val, const auto &min,
                          const auto &max) {
    if (val > max || ((val < min) & (val != 0))) {
      ignoring << ' ' << p;
      return !(invalidPoints = true);
    }

    return true;
  };

  /// \return True if point has either an RPM (having been tested) or a PWM
  auto validPoint = [&](fan::Point &p) {
    // PWM must valid & within bounds
    if (p.validPWM())
      return withinBounds(p, p.pwm, c.pwm_min, pwm_max_abs);

    // RPM must be valid, within bounds and tested
    if (tested && p.validRPM()) {
      // Convert percent to RPM
      if (p.is_rpm_percent && p.rpm > 0) { // RPM is 0 if percent is 0
        const auto multi = static_cast<double>(p.rpm) / 100;
        p.rpm = c.rpm_min + static_cast<rpm_t>(multi * (c.rpm_max - c.rpm_min));
      }

      if (withinBounds(p, p.rpm, c.rpm_min, c.rpm_max)) {
        p.pwm = calcPWM(c.rpm_min, c.pwm_min, c.slope, p.rpm);
        return true;
      }
    } else if (!tested) {
      ignoring << ' ' << p;
      untestedPoints = true;
    }

    return false;
  };

  // Erase invalid points
  points.erase(
      std::remove_if(points.begin(), points.end(),
                     [&validPoint](fan::Point &p) { return !validPoint(p); }),
      points.end());

  if (!ignoring.str().empty()) {
    stringstream uss;
    uss << fanUID;
    string spaces(uss.str().size(), ' ');

    if (untestedPoints)
      LOG(llvl::warning) << uss.str() << " : has not been tested yet, "
            "so RPM (or %) cannot be used - only PWM";

    if (invalidPoints)
      LOG(llvl::warning)
        << ((untestedPoints) ? spaces : uss.str())
        << " : invalid config entries: "
        << "rpm min=" << c.rpm_min << "(or 0), max=" << c.rpm_max
        << "; pwm min=" << c.pwm_min << "(or 0), max=" << pwm_max_abs;

    LOG(llvl::warning) << spaces << " : ignoring" << ignoring.str();
  }
}

pwm_t FanInterface::testPWM(const FanCharacteristics &c, const rpm_t &rpm) {
  // Assume the calculation is the real pwm for a starting point
  auto nextPWM(calcPWM(c.rpm_min, c.pwm_min, c.slope, rpm));

  writePWM(nextPWM);
  sleep_for(wait_time);

  rpm_t curRPM;
  pwm_t curPWM = 0, lastPWM = -1;
  // Break when RPM found when matching, or if between the 5 PWM margin
  while ((curRPM = readRPM()) != rpm && nextPWM != lastPWM) {
    lastPWM = curPWM;
    curPWM = nextPWM;
    nextPWM = (curRPM < rpm) ? nextPWM + 2 : nextPWM - 2;
    if (nextPWM < 0)
      nextPWM = calcPWM(c.rpm_min, c.pwm_min, c.slope, rpm);
    if (curRPM <= 0)
      nextPWM = pwm_start;

    writePWM(nextPWM);
    sleep_for(wait_time);
  }

  return nextPWM;
}

/// \retval state FanState::FULL_SPEED
rpm_t FanInterface::getMaxRPM(FanState &state, const milliseconds &waitTime) {
  // Set fan to full speed, then return the rpm once it has stopped changing
  writePWM(pwm_max_abs);
  sleep_for(waitTime);

  rpm_t rpmMax{};
  for (rpm_t prevRPMMax = -1; rpmMax > prevRPMMax; sleep_for(waitTime)) {
    prevRPMMax = rpmMax;
    rpmMax = readRPM();
  }

  state = FanState::full_speed;
  return rpmMax;
}

/// \return The maximum time to reach full speed, or stop the fan (larger of the
/// two)
/// \retval state FanState::FULL_SPEED
milliseconds FanInterface::getMaxSpeedChangeTime(FanState &state,
                                                 const rpm_t &rpmMax) {
  milliseconds stopTime;
  {
    assert(state == FanState::full_speed);

    // Stop fan, and measure time for fan to stop
    writePWM(pwm_min_abs);
    auto pre = chrono::steady_clock::now();
    while (readRPM() > 0);
    auto post = chrono::steady_clock::now();

    state = FanState::stopped;
    stopTime = chrono::duration_cast<milliseconds>(post - pre);
  }

  milliseconds startTime;
  {
    assert(state == FanState::stopped);

    // Test time taken for RPM to reach max
    auto pre = chrono::steady_clock::now();
    writePWM(pwm_max_abs);
    while (readRPM() < rpmMax);
    auto post = chrono::steady_clock::now();

    state = FanState::full_speed;
    startTime = chrono::duration_cast<milliseconds>(post - pre);
  }

  return (stopTime > startTime) ? stopTime : startTime;
}

/// \retval state FanState::SPINNING
pwm_t FanInterface::getMaxPWM(FanState &state, const milliseconds &waitTime,
                              const rpm_t &rpmMax) {
  assert(state == FanState::full_speed);

  auto pwmMax = pwm_max_abs;
  for (; (rpmMax - 5) <= readRPM(); sleep_for(waitTime)) {
    pwmMax -= 2;
    writePWM(pwmMax - 2);
  }

  state = FanState::unknown; // Almost full_speed
  return pwmMax;
}

/// \return Minimum PWM value required to start the fan
/// \retval state FanState::SPINNING
pwm_t FanInterface::getPWMStart(FanState &state, const milliseconds &waitTime) {
  // Stop fan
  writePWM(pwm_min_abs);
  while (readRPM() > 0)
    sleep_for(waitTime);
  //  state = FanState::stopped;

  //  assert(state == FanState::stopped);
  // Find PWM at which fan starts
  pwm_t pwmStart = 0;
  while (readRPM() <= 0) {
    writePWM((pwmStart += 5));
    sleep_for(waitTime);
  }

  // Increase start PWM by an arbitrary amount to ensure start
  constexpr const pwm_t arbInc = 10;
  if ((pwmStart + arbInc) <= pwm_max_abs)
    pwmStart += arbInc;
  else
    pwmStart = pwm_max_abs;

  state = FanState::unknown;
  return pwmStart;
}

/// \return Minimum PWM & RPM, prior to the fan stopping
/// \retval state FanState::STOPPED
tuple<pwm_t, rpm_t> FanInterface::getMinAttributes(FanState &state,
                                                   const milliseconds &waitTime,
                                                   const pwm_t &startPWM) {
  assert(state != FanState::stopped);
  // Begin at the startPWM, to reduce time taken
  pwm_t pwm_min(startPWM);
  writePWM(pwm_min);

  rpm_t rpm_min{};
  for (rpm_t rpm; (rpm = readRPM()) > 0; sleep_for(waitTime)) {
    // Break if min RPM has already been found
    if (rpm < rpm_min) // Fans may increase their rpm
      break;

    rpm_min = rpm;
    pwm_min -= 2;
    writePWM(pwm_min - 2);
  }

  state = FanState::stopped;
  return std::tie(pwm_min, rpm_min);
}
