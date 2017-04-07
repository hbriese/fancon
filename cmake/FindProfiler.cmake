# Find Profiler
# -------------
# Find (Google Perf Tools) Profiler's header and library
#
# Documentation is available at: gperftools.github.io/gperftools/cpuprofile.html
#
#   PROFILER_INCLUDE_DIR - Path to header (profiler.h)
#   PROFILER_LIBRARY     - Path to library (profiler)
#   PROFILER_FOUND       - True if both PROFILER_DIR & PROFILER_LIBRARY are found

find_path(PROFILER_INCLUDE_DIR
        NAMES profiler.h
        PATH_SUFFIXES gperftools google)

find_library(PROFILER_LIBRARY
        NAMES profiler)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Profiler DEFAULT_MSG
        PROFILER_INCLUDE_DIR PROFILER_LIBRARY)