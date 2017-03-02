#ifndef FANCON_FANNV_HPP
#define FANCON_FANNV_HPP

#include <pstreams/pstream.h>
#include <X11/Xlib.h>
#include <NVCtrl/NVCtrl.h>
#include <NVCtrl/NVCtrlLib.h>
#include "FanInterface.hpp"

namespace fancon {
struct DisplayWrapper {
  DisplayWrapper();
  ~DisplayWrapper() {
    if (dp)
      XCloseDisplay(dp);
  }
  Display *operator*();
  Display *dp;
  bool open() const { return dp != NULL; }
};

static DisplayWrapper dw;

namespace NV {
struct Data_R {
  Data_R(const char *title, const uint attribute, const int target = NV_CTRL_TARGET_TYPE_COOLER)
      : title(title), target(target), attribute(attribute) {}

  const string title;
  const int target;
  const uint attribute;

  int read(const int hwID) const;
};

struct Data_RW : Data_R {
  using Data_R::Data_R;

  void write(const int hwID, int val) const {
    if (!XNVCTRLSetTargetAttributeAndGetStatus(*dw, target, hwID, 0, attribute, val))
      LOG(severity_level::error) << "NVIDIA fan " << hwID << ": failed writing " << title << " = " << val;
  }
};

static const Data_R rpm("RPM", NV_CTRL_THERMAL_COOLER_SPEED);
static const Data_RW pwm_percent("PWM %", NV_CTRL_THERMAL_COOLER_LEVEL);
static const Data_RW enable_mode("Fan speed enable mode", NV_CTRL_GPU_COOLER_MANUAL_CONTROL, NV_CTRL_TARGET_TYPE_GPU);
}

class FanNV : public FanInterface {
public:
  FanNV(const UID &fanUID, const FanConfig &conf = FanConfig(), bool dynamic = true);
  ~FanNV() { writeEnableMode(driver_enable_mode); }

//  using FanInterface::test;
//  using FanInterface::writeTestResult;

  int readRPM() { return NV::rpm.read(hwID); };
  int readPWM() { return percentToPWM(NV::pwm_percent.read(hwID)); }
  void writePWM(int pwm) { NV::pwm_percent.write(hwID, pwmToPercent(pwm)); }
  void writeEnableMode(int mode) { NV::enable_mode.write(hwID, mode); }

private:
  static int pwmToPercent(int pwm);
  static int percentToPWM(int percent);
};
}
#endif //FANCON_FANNV_HPP
