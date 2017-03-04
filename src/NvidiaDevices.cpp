#ifdef FANCON_NVIDIA_SUPPORT
#include "NvidiaDevices.hpp"

using namespace fancon;

DisplayWrapper::DisplayWrapper() {
  const char *denv = "DISPLAY", *xaenv = "XAUTHORITY";

  redi::ipstream ips("echo \"$(ps wwaux 2>/dev/null | grep -wv PID | grep -v grep)\" "
                         "| grep '/X.* :[0-9][0-9]* .*-auth' | egrep -v 'startx|xinit' "
                         "| sed -e 's,^.*/X.* \\(:[0-9][0-9]*\\) .* -auth \\([^ ][^ ]*\\).*$,\\1\\,\\2,' | sort -u");
  string pair;
  ips >> pair;
  auto sepIt = find(pair.begin(), pair.end(), ',');
  if (!pair.empty() || sepIt != pair.end()) {
    string da(pair.begin(), sepIt), xa(sepIt + 1, pair.end());
    setenv(denv, da.c_str(), 1);
    setenv(xaenv, xa.c_str(), 1);
  }

  // try :0 if $DISPLAY is not set
  bool forceDEnv;
  if ((forceDEnv = !getenv(denv)))
    setenv(denv, ":0", 1);

  if (!(dp = XOpenDisplay(NULL))) {
    // set log as debug only, so headless users don't get spammed
    LOG(severity_level::debug) << "Failed to connect to X11 display";

    if (forceDEnv)
      LOG(severity_level::debug) << "Set \"display=\" in " << Util::conf_path
                                 << " to your display (echo $" << denv << ')';
    if (!getenv(xaenv))
      LOG(severity_level::debug) << "Set \"xauthority=\" in " << Util::conf_path
                                 << " to your .Xauthority file (echo $" << xaenv << ')';
  }
}

Display *DisplayWrapper::operator*() {
  if (!dp) {
    const char *err = "Dereferenced null Display* in DisplayWrapper";
    LOG(severity_level::fatal) << err;
    exit(EXIT_FAILURE);
  }

  return dp;
}

int NV::Data_R::read(const int hwID) const {
  int val{};
  if (!XNVCTRLQueryTargetAttribute(*dw, target, hwID, 0, attribute, &val)) {
    LOG(severity_level::error) << "NVIDIA fan " << hwID << ": failed reading " << title;
    val = 0;
  }

  return val;
}

void NV::Data_RW::write(const int hwID, int val) const {
  if (!XNVCTRLSetTargetAttributeAndGetStatus(*dw, target, hwID, 0, attribute, val))
    LOG(severity_level::error) << "NVIDIA fan " << hwID << ": failed writing " << title << " = " << val;
}

FanNV::FanNV(const UID &fanUID, const FanConfig &conf, bool dynamic)
    : FanInterface(fanUID, conf, dynamic, 0) {
  if (!points.empty()) {
    // set manual mode
    writeEnableMode(manual_enable_mode);
    if (readRPM() <= 0)  // start fan if stopped
      writePWM(pwm_start);
  }
}

int FanNV::pwmToPercent(int pwm) { return static_cast<int>((static_cast<double>(pwm) / pwm_max_absolute) * 100); }

int FanNV::percentToPWM(int percent) { return static_cast<int>(static_cast<double>(percent / 100) * pwm_max_absolute); }

bool SensorParentNV::operator==(const UID &other) const { return hwID == other.hwID; }
#endif //FANCON_NVIDIA_SUPPORT