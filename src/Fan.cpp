#include "Fan.hpp"

using namespace fancon;

Fan::Fan(const UID &fanUID, const FanConfig &conf, bool dynamic)
    : FanInterface(fanUID, conf, dynamic, 2), hwIDStr(to_string(hwID)) {
  FanPaths p(fanUID);

  // keep paths (with changing values) for future rw
  pwm_p = p.pwm_p;
  rpm_p = p.rpm_p;
  enable_pf = p.enable_pf;

  // Correct driver enable mode if it is not default
  int em;
  if ((em = readEnableMode()) > driver_enable_mode)
    driver_enable_mode = em;

  if (!points.empty()) {
    // set manual mode
    writeEnableMode(manual_enable_mode);
    if (readRPM() <= 0)  // start fan if stopped
      writePWM(pwm_start);
  }
}

int Fan::testPWM(int rpm) {
  // assume the calculation is the real pwm for a starting point
  auto nextPWM(calcPWM(rpm));

  writePWM(nextPWM);
  sleep(speed_change_t * 3);

  int curPWM = 0, lastPWM = -1, curRPM;
  // break when RPM found when matching, or if between the 5 PWM margin
  while ((curRPM = readRPM()) != rpm && nextPWM != lastPWM) {
    lastPWM = curPWM;
    curPWM = nextPWM;
    nextPWM = (curRPM < rpm) ? nextPWM + 2 : nextPWM - 2;
    if (nextPWM < 0)
      nextPWM = calcPWM(rpm);
    if (curRPM <= 0)
      nextPWM = pwm_start;

    writePWM(nextPWM);
    sleep(speed_change_t);
  }

  return nextPWM;
}

//FanTestResult Fan::test(const UID &fanUID) {
//  FanPaths p(fanUID);
//  string hwID = to_string(fanUID.hwID);
//
//  auto rRPM = [&p]() { return read<int>(p.rpm_p); };
//  auto rPWM = [&p]() { return read<int>(p.pwm_p); };
//  auto wPWM = [&p](int pwm) { return write<int>(p.pwm_p, pwm); };
//  auto rEnableMode = [&p, &hwID]() { return read<int>(p.enable_pf, hwID, DeviceType::FAN, true); };
//  auto wEnableMode = [&p, &hwID](int mode) { return write<int>(p.enable_pf, hwID, mode, DeviceType::FAN, true); };
//
//  // store pre-test (likely driver controlled) fan mode
//  int prevEnableMode = rEnableMode();
//
//  return tests::runTests(rPWM, wPWM, rRPM, wEnableMode, manual_enable_mode, prevEnableMode);
//}