#ifdef FANCON_NVIDIA_SUPPORT
#include "NvidiaUtil.hpp"
#include "SensorInterface.hpp"

using namespace fc;

namespace fc {
namespace NV {
unique_ptr<NV::LibXNvCtrl> xnvlib;
}
} // namespace fc

optional<int> NV::NVAttr_R::read(int id) const {
  int val;
  if (!xnvlib->QueryTargetAttribute(*NV::xnvlib->xdisplay, target, id, 0,
                                    attribute, &val)) {
    LOG(llvl::error) << "NVIDIA device " << id << ": failed reading " << title;
    return nullopt;
  }

  return val;
}

NV::DynamicLibrary::DynamicLibrary(const char *file)
    : handle(dlopen(file, RTLD_LAZY)) {
  if (!handle)
    LOG(llvl::error) << "Failed to dload " << file << ": " << dlerror();
}

bool NV::DynamicLibrary::available() const { return handle != nullptr; }

#define GET_SYMBOL(_proc, _name)                                               \
  _proc = (decltype(_proc))(dlsym(handle, _name));                             \
  if (!_proc)                                                                  \
    LOG(llvl::error) << "Failed to load symbol " << _name;

NV::X11Display::X11Display() : DynamicLibrary("libX11.so") {
  if (!available())
    return;

  GET_SYMBOL(OpenDisplay, "XOpenDisplay");
  GET_SYMBOL(CloseDisplay, "XCloseDisplay");
  GET_SYMBOL(InitThreads, "XInitThreads");

  init_display();
}

NV::LibXNvCtrl::LibXNvCtrl()
    : DynamicLibrary("libXNVCtrl.so"), xdisplay(X11Display()),
      rpm("Rpm", NV_CTRL_THERMAL_COOLER_SPEED),
      temp("temperature", NV_CTRL_THERMAL_SENSOR_READING,
           NV_CTRL_TARGET_TYPE_THERMAL_SENSOR),
      pwm_percent("Pwm %", NV_CTRL_THERMAL_COOLER_LEVEL),
      enable_mode("fan control enable mode", NV_CTRL_GPU_COOLER_MANUAL_CONTROL,
                  NV_CTRL_TARGET_TYPE_GPU) {
  if (available()) {
    GET_SYMBOL(QueryExtension, "XNVCTRLQueryExtension");
    GET_SYMBOL(QueryVersion, "XNVCTRLQueryVersion");
    GET_SYMBOL(QueryTargetCount, "XNVCTRLQueryTargetCount");
    GET_SYMBOL(QueryTargetBinaryData, "XNVCTRLQueryTargetBinaryData");
    GET_SYMBOL(QueryTargetStringAttribute, "XNVCTRLQueryTargetStringAttribute");
    GET_SYMBOL(QueryTargetAttribute, "XNVCTRLQueryTargetAttribute");
    GET_SYMBOL(SetTargetAttributeAndGetStatus,
               "XNVCTRLSetTargetAttributeAndGetStatus");
  }
  supported = check_support();
}

#undef GET_SYMBOL

NV::X11Display::~X11Display() {
  if (dp)
    CloseDisplay(dp);
}

Display *NV::X11Display::operator*() {
  if (!connected())
    throw runtime_error("X11 display couldn't be opened "
                        "but is being used anyway!");

  return dp;
}

bool NV::X11Display::connected() const { return dp != nullptr; }

void NV::X11Display::init_display() {
  // Try find the display from environmental variables & ensure xauth is set
  string dsp;
  if (char *res; (res = getenv("DISPLAY")))
    dsp = string(res);

  // If not found, attempt to find xauth, and or display from running X11 server
  bool found_xauth = getenv("XAUTHORITY") != nullptr;
  if (dsp.empty() || !found_xauth) {
    if (dsp.empty())
      LOG(llvl::debug) << "Guessing X11 env var $DISPLAY, consider setting";
    if (!found_xauth)
      LOG(llvl::debug) << "Guessing X11 env var $XAUTHORITY, consider setting";

    redi::ipstream ips(
        "echo \"$(ps wwaux 2>/dev/null | grep -wv PID | grep -v grep)\" "
        "| grep '/X.* :[0-9][0-9]* .*-auth' | egrep -v 'startx|xinit' "
        "| sed -e 's,^.*/X.* \\(:[0-9][0-9]*\\) .* -auth \\([^ ][^ "
        "]*\\).*$,\\1\\,\\2,' | sort -u");
    string dsp_xauth;
    ips >> dsp_xauth;

    // Set missing dsp/xa to that found
    const auto sep_it = find(dsp_xauth.begin(), dsp_xauth.end(), ',');
    if (sep_it != dsp_xauth.end()) {
      if (dsp.empty())
        dsp = string(dsp_xauth.begin(), sep_it);

      if (!found_xauth) {
        setenv("XAUTHORITY", string(next(sep_it), dsp_xauth.end()).c_str(), 1);
        found_xauth = true;
      }
    }
  }

  // Open display with dsp or guess at display if not set
  InitThreads();
  dp = OpenDisplay((!dsp.empty() ? dsp.c_str() : nullptr));

  if (!dp) {
    LOG(llvl::debug) << Util::join({{!dsp.empty(), "$DISPLAY"},
                                    {!found_xauth, "$XAUTHORITY"}})
                     << " env variable(s) not set, set explicitly to "
                        "enable NVIDIA control";
  }
}

uint NV::LibXNvCtrl::get_num_GPUs() {
  if (!supported)
    return 0;

  // Query number of GPUs
  int n_gpus{0};
  if (!QueryTargetCount(*xdisplay, NV_CTRL_TARGET_TYPE_GPU, &n_gpus)) {
    LOG(llvl::error) << "Failed to query number of NVIDIA GPUs";
    return 0;
  }

  return n_gpus;
}

string NV::LibXNvCtrl::get_gpu_product_name(NVID gpu_id) {
  // GPU product name
  char *name_buf = nullptr;
  int ret =
      QueryTargetStringAttribute(*xdisplay, NV_CTRL_TARGET_TYPE_GPU, gpu_id, 0,
                                 NV_CTRL_STRING_PRODUCT_NAME, &name_buf);

  if (!ret || name_buf == nullptr) {
    LOG(llvl::error) << "Failed to query gpu: " << gpu_id << " product name";
    return "";
  }

  string prod_name(name_buf);

  // Move iterator past title(s), and space trailing it
  vector<string> titles = {"GeForce", "GTX", "GRID", "Quadro"};
  auto begIt = prod_name.begin();
  for (const auto &t : titles) {
    auto newBegIt =
        search(begIt, prod_name.end(), t.begin(), t.end()) + t.size();
    if (*newBegIt == ' ' && (newBegIt + 1) != prod_name.end())
      begIt = ++newBegIt;
  }

  std::replace(begIt, prod_name.end(), ' ', '_');

  return string(begIt, prod_name.end());
}

vector<uint> NV::LibXNvCtrl::from_binary_data(const unsigned char *data,
                                              int len) {
  vector<uint> v;
  for (int i{0}; i < len && *(data + i) != '\0'; ++i) {
    // Return an index (start at 0), where as binary data start at 1
    v.push_back(static_cast<uint>(*(data + i)) - 1);
  }

  return v;
}

bool NV::LibXNvCtrl::check_support() {
  if (!available()) {
    LOG(llvl::debug) << "libXNVCtrl unavailable";
    return false;
  }

  if (!xdisplay.connected()) {
    LOG(llvl::debug) << "X11 display cannot be opened";
    return false;
  }

  int eventBase, errorBase;
  if (!QueryExtension(*xdisplay, &eventBase, &errorBase)) {
    return false; // NV-CONTROL X doesn't exist
  } else if (errorBase)
    LOG(llvl::warning) << "NV-CONTROL X return error base: " << errorBase;

  int major{0}, minor{0};
  if (!QueryVersion(*xdisplay, &major, &minor)) {
    LOG(llvl::error) << "Failed to query NV-CONTROL X version";
    return false;
  } else if ((major < 1) || (major == 1 && minor < 9)) // v1.9 minimum
    LOG(llvl::warning) << "NV-CONTROL X version too old (< 1.9), "
                       << "update your NVIDIA driver";

  // Enable fan control coolbit if start from a TTY - so user knows to reboot
  if (Util::is_atty() && enable_fan_control_coolbit()) {
    LOG(llvl::warning)
        << "Enabling NVIDIA fan control coolbits flag, restart for NV support";
    return false;
  }

  return true;
}

bool NV::LibXNvCtrl::enable_fan_control_coolbit() {
  redi::ipstream ips("nvidia-xconfig -t | grep Coolbits");
  string l;
  std::getline(ips, l);

  // Exit early with message if nvidia-xconfig isn't found
  if (l.empty()) {
    LOG(llvl::debug) << "Couldn't find nvidia-xconfig. NVIDIA coolbits bit (4) "
                        "may or may not be set";
    return false;
  }

  const auto initialv = Util::postfix_num<uint>(l).value_or(0);
  uint curv = initialv;

  const auto nBits = 5;
  const auto fcBit = 2;                 // 4
  uint cbVal[nBits] = {1, 2, 4, 8, 16}; // bit^2
  bool cbSet[nBits]{false};

  // Determine set coolbit values
  for (auto i = nBits - 1; i >= 0; --i)
    if ((cbSet[i] = (curv / cbVal[i]) >= 1))
      curv -= cbVal[i];

  // Value malformed if there are remaining 'bits'
  auto newv = initialv;
  if (curv > 0) {
    LOG(llvl::error) << "Fixing invalid coolbits value";
    newv -= curv;
  }

  // Increment coolbits value if manual fan control coolbit not set
  if (!cbSet[fcBit])
    newv += cbVal[fcBit];

  // Write new value if changes to the initial value have been made
  if (newv != initialv) {
    LOG(llvl::info) << "Enabling NVIDIA fan control coolbit; value: " << newv;

    const auto scb = string("nvidia-xconfig -a --cool-bits=") + to_string(newv);
    if (system(scb.c_str()) != 0)
      LOG(llvl::error)
          << "Failed to write coolbits value, nvidia fan control may fail!";

    return true;
  }

  return false;
}

bool NV::init(bool force) {
  if (!NV::xnvlib || force)
    NV::xnvlib = make_unique<NV::LibXNvCtrl>();

  return bool(NV::xnvlib);
}

#endif // FANCON_NVIDIA_SUPPORT
