#include "FanNV.hpp"

using namespace fancon;

DisplayWrapper::DisplayWrapper() {
  redi::ipstream ips("echo \"$(ps wwaux 2>/dev/null | grep -wv PID | grep -v grep)\" "
                         "| grep '/X.* :[0-9][0-9]* .*-auth' | egrep -v 'startx|xinit' "
                         "| sed -e 's,^.*/X.* \\(:[0-9][0-9]*\\) .* -auth \\([^ ][^ ]*\\).*$,\\1\\,\\2,' | sort -u");
  string pair;
  ips >> pair;
  auto sepIt = find(pair.begin(), pair.end(), ',');
  if (!pair.empty() || sepIt != pair.end()) {
    string da(pair.begin(), sepIt), xa(sepIt + 1, pair.end());
    setenv("DISPLAY", da.c_str(), 1);
    setenv("XAUTHORITY", xa.c_str(), 1);
  }
  // TODO: check that values are legit
  if (!(dp = XOpenDisplay(NULL)))
    LOG(severity_level::error) << "Failed to connect to X11 display: "
                               << "$DISPLAY " << getenv("DISPLAY") << " $XAUTHORITY " << getenv("XAUTHORITY");
}

Display *DisplayWrapper::operator*() {
  if (!dp) {
    const char *err = "Dereferenced null Display* in DisplayWrapper";
    LOG(severity_level::fatal) << err;
    exit(EXIT_FAILURE);
  }

  return dp;
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

int NV::Data_R::read(const int hwID) const {
  int val{};
  if (!XNVCTRLQueryTargetAttribute(*dw, target, hwID, 0, attribute, &val)) {
    LOG(severity_level::error) << "NVIDIA fan " << hwID << ": failed reading " << title;
    val = 0;
  }

  return val;
}