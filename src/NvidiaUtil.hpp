#ifdef FANCON_NVIDIA_SUPPORT
#ifndef FANCON_NVIDIAUTIL_HPP
#define FANCON_NVIDIAUTIL_HPP

#include <pstreams/pstream.h>
#include <dlfcn.h>  // dlopen
#include <X11/Xlib.h>     // TODO: replace Xlib & NVCtrl libs with NVML once it supports setting fan speed
#include <NVCtrl/NVCtrlLib.h>
#include "Logging.hpp"
#include "FanInterface.hpp"
#include "SensorInterface.hpp"

#ifdef FANCON_NVML_SUPPORT_EXPERIMENTAL
#include <nvml.h>
#endif //FANCON_NVML_SUPPORT_EXPERIMENTAL

namespace fancon {
namespace NV {
#define GET_SYMBOL(_proc, _name)                               \
    _proc = (decltype(_proc)) dlsym(handle, _name);            \
    if (!_proc)                                                \
        LOG(llvl::debug) << "Failed to load symbol " << _name; \

struct DynamicLibrary {
  DynamicLibrary(const char *file)
      : handle(dlopen(file, RTLD_LAZY | RTLD_LOCAL)) {
    if (!handle)
      LOG(llvl::debug) << "Failed to load " << file << ": " << dlerror() << "; see `fancon --help` for more info";
  }
  ~DynamicLibrary() { dlclose(handle); }

  void *handle;
};

struct LibX11 : public DynamicLibrary {
  LibX11() : DynamicLibrary("libX11.so") {
    if (handle) {
      GET_SYMBOL(OpenDisplay, "XOpenDisplay");
      GET_SYMBOL(CloseDisplay, "XCloseDisplay");
      GET_SYMBOL(InitThreads, "XInitThreads");
    }
  }

  // Function names exclude the 'X' prefix
  decltype(XOpenDisplay)    (*OpenDisplay);
  decltype(XCloseDisplay)   (*CloseDisplay);
  decltype(XInitThreads)    (*InitThreads);
} extern xlib;

struct LibXNvCtrl : public DynamicLibrary {
  LibXNvCtrl() : DynamicLibrary("libXNVCtrl.so") {
    if (handle) {
      GET_SYMBOL(QueryExtension, "XNVCTRLQueryExtension");
      GET_SYMBOL(QueryVersion, "XNVCTRLQueryVersion");
      GET_SYMBOL(QueryTargetCount, "XNVCTRLQueryTargetCount");
      GET_SYMBOL(QueryTargetBinaryData, "XNVCTRLQueryTargetBinaryData");
      GET_SYMBOL(QueryTargetStringAttribute, "XNVCTRLQueryTargetStringAttribute");
      GET_SYMBOL(QueryTargetAttribute, "XNVCTRLQueryTargetAttribute");
      GET_SYMBOL(SetTargetAttributeAndGetStatus, "XNVCTRLSetTargetAttributeAndGetStatus");
    }
  }

  // Function names exclude the "XNVCTRL" prefix
  decltype(XNVCTRLQueryExtension)                   (*QueryExtension);
  decltype(XNVCTRLQueryVersion)                     (*QueryVersion);
  decltype(XNVCTRLQueryTargetCount)                 (*QueryTargetCount);
  decltype(XNVCTRLQueryTargetBinaryData)            (*QueryTargetBinaryData);
  decltype(XNVCTRLQueryTargetStringAttribute)       (*QueryTargetStringAttribute);
  decltype(XNVCTRLQueryTargetAttribute)             (*QueryTargetAttribute);
  decltype(XNVCTRLSetTargetAttributeAndGetStatus)   (*SetTargetAttributeAndGetStatus);
} extern xnvlib;
#undef GET_SYMBOL

struct DisplayWrapper {
  DisplayWrapper(Display *dsp) { dp = dsp; }
  ~DisplayWrapper() {
    if (dp)
      xlib.CloseDisplay(dp);
  }
  Display *operator*();
  Display *dp;
  bool open(string display, string xauthority);
} extern dw;

extern const bool support;
bool supported();
bool enableFanControlCoolbit();    // Doesn't work in snap confined

int getNumGPUs();
vector<int> nvProcessBinaryData(const unsigned char *data, const int len);
vector<UID> getFans();
vector<UID> getSensors();

#ifdef FANCON_NVML_SUPPORT_EXPERIMENTAL
vector<nvmlDevice_t> getDevices();
vector<UID> getFans();
vector<UID> getSensors();
#endif //FANCON_NVML_SUPPORT_EXPERIMENTAL

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

  template<typename T>
  bool write(const int hwID, const T &value) const {
    if (!xnvlib.SetTargetAttributeAndGetStatus(*dw, target, hwID, 0, attribute, value)) {
      LOG(llvl::debug) << "NVIDIA fan " << hwID << ": failed writing " << title << " = " << value;
      return false;
    }

    return true;
  }
};

static const Data_R rpm("RPM", NV_CTRL_THERMAL_COOLER_SPEED);
static const Data_RW pwm_percent("PWM %", NV_CTRL_THERMAL_COOLER_LEVEL);
static const Data_RW enable_mode("Fan speed enable mode", NV_CTRL_GPU_COOLER_MANUAL_CONTROL, NV_CTRL_TARGET_TYPE_GPU);
static const Data_R temp("temperature", NV_CTRL_THERMAL_SENSOR_READING, NV_CTRL_TARGET_TYPE_THERMAL_SENSOR);
}
}

#endif //FANCON_NVIDIA_SUPPORT
#endif //FANCON_NVIDIAUTIL_HPP
