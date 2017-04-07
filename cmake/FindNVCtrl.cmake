# FindNVCtrl
# -------------
# Find the NV-CONTROL X Extension headers and library
#
# Documentation is available at: github.com/NVIDIA/nvidia-settings/blob/master/doc/NV-CONTROL-API.txt
#
# License issue:
# LibXNvCtrl is intended to be MIT-X11 / BSD but is marked as GPL in older
# driver versions.
# This issue has been raised in this thread:
# http://www.nvnews.net/vbulletin/showpost.php?p=1756087&postcount=8
# it has been fixed in drivers 177.82 and above
#
#   NVCTRL_INCLUDE_DIR - Path to headers (NVCtrlLib.h NVCtrl.h)
#   NVCTRL_LIBRARY     - Path to library (XNVCtrl)
#   NVCTRL_FOUND       - True if both NVCTRL_INCLUDE_DIR & NVCTRL_LIBRARY are found

find_path(NVCTRL_INCLUDE_DIR
        NAMES NVCtrlLib.h NVCtrl.h
        PATH_SUFFIXES NVCtrl)

find_library(NVCTRL_LIBRARY
        NAMES XNVCtrl)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NVCtrl DEFAULT_MSG
        NVCTRL_INCLUDE_DIR NVCTRL_LIBRARY)