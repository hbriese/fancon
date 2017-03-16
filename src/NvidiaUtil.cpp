#ifdef FANCON_NVIDIA_SUPPORT
#include "NvidiaUtil.hpp"

using namespace fancon;

// define global DisplayWrapper
namespace fancon {
namespace NV {
NV::DisplayWrapper dw(NULL);
}
}

void NV::DisplayWrapper::open(string da, string xa) {
  const char *denv = "DISPLAY", *xaenv = "XAUTHORITY";
  bool forcedDa = false;

  // Set da and xa to environmental variables if they are set
  char *res;
  if (da.empty() && (res = getenv(denv)))
    da = string(res);
  if (xa.empty() && (res = getenv(xaenv)))
    xa = string(res);

  if ((da.empty()) || (xa.empty())) {
    // Attempt to find the da and xa used by startx | xinit
    redi::ipstream ips("echo \"$(ps wwaux 2>/dev/null | grep -wv PID | grep -v grep)\" "
                           "| grep '/X.* :[0-9][0-9]* .*-auth' | egrep -v 'startx|xinit' "
                           "| sed -e 's,^.*/X.* \\(:[0-9][0-9]*\\) .* -auth \\([^ ][^ ]*\\).*$,\\1\\,\\2,' | sort -u");
    string pair;
    ips >> pair;

    // set missing da/xa to that found
    auto sepIt = find(pair.begin(), pair.end(), ',');
    if (sepIt != pair.end()) {

      if (da.empty()) {
        da = string(pair.begin(), sepIt);
      }
      if (xa.empty()) {
        xa = string(sepIt + 1, pair.end());
        setenv(xaenv, xa.c_str(), 1);   // $XAUTHORITY is queried directly by XOpenDisplay
      }
    } else if ((forcedDa = da.empty()))    // if no da is found, then guess
      da = ":0";
  }

  // Open display and log errors
  if (!(dp = XOpenDisplay(da.c_str()))) {
    stringstream err;

    if (forcedDa)   // da.empty() == true   // TODO: review check - guessed da may be correct
      err << "Set \"display=\" in " << Util::conf_path << " to your display (echo $" << denv << ')';
    if (!getenv(xaenv))
      err << "Set \"xauthority=\" in " << Util::conf_path << " to your .Xauthority file (echo $" << xaenv << ')';

    LOG(llvl::debug) << "Failed to open X11 display: " << err.str();
  }
}

Display *NV::DisplayWrapper::operator*() {
  assert(dp != NULL);
  return dp;
}

bool NV::supported() {
  if (!Util::locked())
    XInitThreads();       // run before first XOpenDisplay() from any process

  dw.open({}, {});  // TODO: support user supplied display and xauthority
  if (!dw.isOpen()) {
    LOG(llvl::debug) << "X11 display cannot be opened";
    return false;
  }

  int eventBase, errorBase;
  auto ret = XNVCTRLQueryExtension(*dw, &eventBase, &errorBase);
  if (!ret) {
    LOG(llvl::debug) << "NVIDIA fan control not supported";   // NV-CONTROL X does not exist!
    return false;
  } else if (errorBase)
    LOG(llvl::warning) << "NV-CONTROL X return error base: " << errorBase;

  int major = 0, minor = 0;
  ret = XNVCTRLQueryVersion(*dw, &major, &minor);
  if (!ret) {
    LOG(llvl::error) << "Failed to query NV-CONTROL X version";
    return false;
  } else if ((major < 1) || (major == 1 && minor < 9))  // must be at least v1.9
    LOG(llvl::warning) << "NV-CONTROL X version is not officially supported (too old!); please use v1.9 or higher";

//  LOG(llvl::debug) << "NVIDIA fan control supported";
  return true;
}

void NV::enableFanControlCoolbit() {
  // TODO: find and set Coolbits value without 'nvidia-xconfig' - for when not available (e.g. in snap confinement)
  string command("sudo nvidia-xconfig -t | grep Coolbits");
  redi::pstream ips(command);
  string l;
  std::getline(ips, l);
  int iv = (!l.empty()) ? Util::getLastNum(l) : 0;  // initial value
  int cv(iv);   // current

  const int nBits = 5;
  const int fcBit = 2;    // 4
  int cbV[nBits] = {1, 2, 4, 8, 16};  // pow(2, bit)
  int cbS[nBits]{0};

  for (auto i = nBits - 1; i >= 0; --i)
    if ((cbS[i] = (cv / cbV[i]) >= 1))
      cv -= cbV[i];

  int nv = iv;
  if (cv > 0) {
    LOG(llvl::error) << "Invalid coolbits value, fixing with fan control bit set";
    nv -= cv;
  }

  if (!cbS[fcBit])
    nv += cbV[fcBit];

  if (nv != iv) {
    command = string("sudo nvidia-xconfig --cool-bits=") + to_string(nv) + " > /dev/null";
    if (system(command.c_str()) != 0)
      LOG(llvl::error) << "Failed to write coolbits value, nvidia fan test may fail!";
    LOG(llvl::info) << "Reboot, or restart your display server to enable NVIDIA fan control";
  }
}

int NV::Data_R::read(const int hwID) const {
  int val{};
  if (!XNVCTRLQueryTargetAttribute(*dw, target, hwID, 0, attribute, &val)) {
    LOG(llvl::error) << "NVIDIA fan " << hwID << ": failed reading " << title;
    val = 0;
  }

  return val;
}

void NV::Data_RW::write(const int hwID, int val) const {
  if (!XNVCTRLSetTargetAttributeAndGetStatus(*dw, target, hwID, 0, attribute, val))
    LOG(llvl::error) << "NVIDIA fan " << hwID << ": failed writing " << title << " = " << val;
}
#endif //FANCON_NVIDIA_SUPPORT