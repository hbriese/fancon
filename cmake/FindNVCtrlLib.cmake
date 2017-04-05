# -----------------------------------------------------------------------------
# Find NVCtrlLib SDK
# Define:
# NVCtrlLib_FOUND
# NVCtrlLib_INCLUDE_DIR
# NVCtrlLib_LIBRARY

# typically, download the source package of nvidia-settings from:
# ftp://download.nvidia.com/XFree86/nvidia-settings/
# once uncompressed, nvidia-settings-1.0/src/LibXNvCtrl directory contents
# LibXNvCtrl.a, NVCtrlLib.h and NVCtrl.h.

# or if it is already installed by the distribution:
# /usr/lib/LibXNvCtrl.a
# /usr/include/NVCtrl/NVCtrl.h
# /usr/include/NVCtrl/NVCtrlLib.h

# License issue:
# LibXNvCtrl is intended to be MIT-X11 / BSD but is marked as GPL in older
# driver versions.
# This issue has been raised in this thread:
# http://www.nvnews.net/vbulletin/showpost.php?p=1756087&postcount=8
# it has been fixed in drivers 177.82 and above

find_library(NVCtrlLib_LIBRARY
        NAMES XNVCtrl
        DOC "Path to NVCONTROL static library (libXNVCtrl.so)")

find_path(NVCtrlLib_INCLUDE_DIR
        NAMES NVCtrlLib.h NVCtrl.h
        PATH_SUFFIXES NVCtrl
        DOC "Path to NVCONTROL headers (NVCtrlLib.h and NVCtrl.h)")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NVCtrlLib DEFAULT_MSG
        NVCtrlLib_INCLUDE_DIR NVCtrlLib_LIBRARY)