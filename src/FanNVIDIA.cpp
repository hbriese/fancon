#include <pstreams/pstream.h>
#include "FanNVIDIA.hpp"

using namespace fancon;

FanNVIDIA::FanNVIDIA(const UID &fanUID, const FanConfig &conf, bool dynamic)
    : FanInterface(fanUID, conf, dynamic) {
  if (!points.empty()) {
    // set manual mode
    writeEnableMode(manual_enable_mode);
    if (readRPM() <= 0)  // start fan if stopped
      writePWM(pwm_start);
  }
}

int FanNVIDIA::readNvVar(const string &hwID, const char *var, bool targetGPU) {
  string command = string("nvidia-settings -q all | grep ") + var + " | grep " + ((targetGPU) ? "gpu:" : "fan:") + hwID
      + " | sed 's/^.*: //'";
  redi::pstream ips(command);
  for (string l; std::getline(ips, l);)
    return Util::getLastNum(l);

  log(LOG_ERR, string(var) + " could not be read, please submit a GitHub issue");
  return 0;
}

FanTestResult FanNVIDIA::test(const UID &fanUID) {
  string hwID = to_string(fanUID.hwID);

  auto rRPM = [&hwID]() { return readRPM(hwID); };
  auto rPWM = [&hwID]() { return percentToPWM(readPWMPercent(hwID)); };
  auto wPWM = [&hwID](int pwm) { return writePWMPercent(hwID, pwmToPercent(pwm)); };
  auto wEnableMode = [&hwID](int mode) { return writeEnableMode(hwID, mode); };

  // store pre-test (likely driver controlled) fan mode
  int prevEnableMode = readEnableMode(hwID);

  return tests::runTests(rPWM, wPWM, rRPM, wEnableMode, manual_enable_mode, prevEnableMode);
}