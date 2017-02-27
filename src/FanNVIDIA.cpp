#include <pstreams/pstream.h>
#include "FanNVIDIA.hpp"

using namespace fancon;

FanNVIDIA::FanNVIDIA(const UID &fanUID, const FanConfig &conf, bool dynamic)
    : FanInterface(fanUID, conf, dynamic) {
  // enable fan control coolbits bit if required
  enableCoolbits();

  if (!points.empty()) {
    // set manual mode
    writeEnableMode(manual_enable_mode);
    if (readRPM() <= 0)  // start fan if stopped
      writePWM(pwm_start);
  }
}

void FanNVIDIA::enableCoolbits() {
  string command("sudo nvidia-xconfig -t | grep Coolbits");
  redi::pstream ips(command);
  string l;
  std::getline(ips, l);
  int iv = (!l.empty()) ? Util::getLastNum(l) : 0;  // initial value
  int cv(iv);   // current

  vector<int> cbV{1, 2, 4, 8, 16};
  int fanControlBit = 2;
  vector<bool> cbS(cbV.size(), 0);

  for (auto i = cbV.size() - 1; i > 0 && cv > 0; --i)
    if ((cbS[i] = (cv / cbV[i]) >= 1))
      cv -= cbV[i];

  int nv = iv;
  if (cv == 0 && !cbS[fanControlBit])
    nv += cbV[fanControlBit];
  else {
    log(LOG_ERR, "Invalid coolbits value, fixing with fan control bit set");
    nv -= cv;
  }

  if (nv != iv) {
    command = string("sudo nvidia-xconfig --cool-bits=") + to_string(nv) + " > /dev/null";
    system(command.c_str());
    log(LOG_NOTICE, "Reboot, or restart your display server to enable NVIDIA fan control");
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