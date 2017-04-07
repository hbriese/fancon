# FindNVML
# --------
# Find the NVIDIA Management Library (NVML) header and library
#
# Documentation is available at: docs.nvidia.com/deploy/nvml-api/index.html
#
#   NVML_INCLUDE_DIR - Path to header (nvml.h)
#   NVML_LIBRARY     - Path to library (nvidia-ml)
#   NVML_FOUND       - True if both NVML_INCLUDE_DIR & NVML_LIBRARY are found

find_path(NVML_INCLUDE_DIR
        NAMES nvml.h
        DOC "Path to the NVML header (nvml.h)")

find_library(NVML_LIBRARY
        NAMES nvidia-ml
        PATH_SUFFIXES nvidia nvidia/current
        HINTS ${CMAKE_HOME_DIRECTORY}/include
        DOC "Path to NVML static library (libnvidia-ml.so or nvidia-ml.so")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NVML DEFAULT_MSG
        NVML_INCLUDE_DIR NVML_LIBRARY)