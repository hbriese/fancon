# - Find TCMalloc
# Find the native Tcmalloc includes and library
#
#  TCMALLOC_INCLUDE_DIR - where to find Tcmalloc.h, etc.
#  TCMALLOC_LIBRARIES   - List of libraries when using Tcmalloc.
#  TCMALLOC_FOUND       - True if Tcmalloc found

find_path(TCMALLOC_INCLUDE_DIR
        NAMES tcmalloc.h
        PATH_SUFFIXES gperftools google)

find_library(TCMALLOC_LIBRARIES
        NAMES libtcmalloc tcmalloc libtcmalloc_minimal tcmalloc_minimal)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TCMalloc DEFAULT_MSG
        TCMALLOC_INCLUDE_DIR TCMALLOC_LIBRARIES)