#ifdef FANCON_NVIDIA_SUPPORT
#ifndef FANCON_NVIDIAUTIL_HPP
#define FANCON_NVIDIAUTIL_HPP

#include <pstreams/pstream.h>
#include <X11/Xlib.h>
#include <NVCtrl/NVCtrl.h>
#include <NVCtrl/NVCtrlLib.h>
#include "FanInterface.hpp"
#include "SensorParentInterface.hpp"

namespace fancon {
namespace NV {
struct DisplayWrapper {
  DisplayWrapper(Display *dsp) { dp = dsp; }
  ~DisplayWrapper() {
    if (dp)
      XCloseDisplay(dp);
  }
  void open(string display, string xauthority);
  Display *operator*();
  Display *dp;
  bool isOpen() const { return dp != NULL; }
} extern dw;  // link to global DisplayWrapper in .cpp

bool supported();
void enableFanControlCoolbit();    // doesn't work when confined

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

  void write(const int hwID, int val) const;
};

static const Data_R rpm("RPM", NV_CTRL_THERMAL_COOLER_SPEED);
static const Data_RW pwm_percent("PWM %", NV_CTRL_THERMAL_COOLER_LEVEL);
static const Data_RW enable_mode("Fan speed enable mode", NV_CTRL_GPU_COOLER_MANUAL_CONTROL, NV_CTRL_TARGET_TYPE_GPU);
static const Data_R temp("temperature", NV_CTRL_THERMAL_SENSOR_READING, NV_CTRL_TARGET_TYPE_THERMAL_SENSOR);
}
}
#endif //FANCON_NVIDIA_SUPPORT
#endif //FANCON_NVIDIAUTIL_HPP
