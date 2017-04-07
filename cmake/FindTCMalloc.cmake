# Find TCMalloc
# -------------
# Find (Google Perf Tools) TCMalloc's header and libraries
#
#   TCMALLOC_INCLUDE_DIR - Path to header (tcmalloc.h)
#   TCMALLOC_LIBRARIES   - Path to libraries (tcmalloc tcmalloc_minimal)
#   TCMALLOC_FOUND       - True if both TCMALLOC_INCLUDE_DIR & TCMALLOC_LIBRARIES are found

find_path(TCMALLOC_INCLUDE_DIR
        NAMES tcmalloc.h
        PATH_SUFFIXES gperftools google)

find_library(TCMALLOC_LIBRARIES
        NAMES tcmalloc tcmalloc_minimal)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TCMalloc DEFAULT_MSG
        TCMALLOC_INCLUDE_DIR TCMALLOC_LIBRARIES)