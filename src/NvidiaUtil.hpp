#ifdef FANCON_NVIDIA_SUPPORT
#ifndef FANCON_NVIDIAUTIL_HPP
#define FANCON_NVIDIAUTIL_HPP

#if 1 // To avoid clang format placing Xlib below NVCtrlLib
#include <X11/Xlib.h>
#endif // 1

#include <NVCtrlLib.h> // WARNING: Xlib must be included before NVCtrlLib

// Xlib.h does not undefine macros, so undefine some of the more troublesome
#ifdef Status
#undef Status
#endif // Status
#ifdef None
#undef None
#endif // None

#include <dlfcn.h> // dlopen
#include <pstreams/pstream.h>

#include "Util.hpp"

using NVID = uint;

namespace fc::NV {
class NVAttr_R {
public:
  NVAttr_R(const char *title, uint attribute,
           int target = NV_CTRL_TARGET_TYPE_COOLER)
      : title(title), target(target), attribute(attribute) {}

  const char *title;
  const int target;
  const uint attribute;

  optional<int> read(int id) const;
};

class NVAttr_RW : public NVAttr_R {
public:
  using NVAttr_R::NVAttr_R;

  template <typename T> bool write(const int id, const T &value) const;
};

class DynamicLibrary {
public:
  explicit DynamicLibrary(const char *file);
  ~DynamicLibrary() { dlclose(handle); }

  bool available() const;

protected:
  void *handle;
};

class X11Display : public DynamicLibrary {
public:
  X11Display();
  ~X11Display();

  Display *operator*();
  bool connected() const;

private:
  Display *dp = nullptr;

  void init_display();

  decltype(XOpenDisplay) *OpenDisplay;
  decltype(XCloseDisplay) *CloseDisplay;
  decltype(XInitThreads) *InitThreads;
};

class LibXNvCtrl : public DynamicLibrary {
public:
  LibXNvCtrl();

  NV::X11Display xdisplay;
  bool supported;
  const NVAttr_R rpm, temp;
  const NVAttr_RW pwm_percent, enable_mode;

  uint get_num_GPUs();
  string get_gpu_product_name(NVID gpu_id);
  static vector<uint> from_binary_data(const unsigned char *data, int len);

  // Function names exclude the "XNVCTRL" prefix
  decltype(XNVCTRLQueryExtension) *QueryExtension;
  decltype(XNVCTRLQueryVersion) *QueryVersion;
  decltype(XNVCTRLQueryTargetCount) *QueryTargetCount;
  decltype(XNVCTRLQueryTargetBinaryData) *QueryTargetBinaryData;
  decltype(XNVCTRLQueryTargetStringAttribute) *QueryTargetStringAttribute;
  decltype(XNVCTRLQueryTargetAttribute) *QueryTargetAttribute;
  decltype(XNVCTRLSetTargetAttributeAndGetStatus)
      *SetTargetAttributeAndGetStatus;

private:
  bool check_support();
  static bool enable_fan_control_coolbit();
};

extern unique_ptr<LibXNvCtrl> xnvlib;

bool init(bool force = false);
} // namespace fc::NV

//----------------------//
// TEMPLATE DEFINITIONS //
//----------------------//

template <typename T>
bool fc::NV::NVAttr_RW::write(const int id, const T &value) const {
  if (!xnvlib->SetTargetAttributeAndGetStatus(*NV::xnvlib->xdisplay, target, id,
                                              0, attribute, value)) {
    LOG(llvl::error) << "NVIDIA fan " << id << ": failed writing " << title
                     << " = " << value;
    return false;
  }

  return true;
}

#endif // FANCON_NVIDIA_SUPPORT
#endif // FANCON_NVIDIAUTIL_HPP
