# FindNVML
# --------
# Find the NVIDIA Management Library (NVML) header and library
#
# Documentation is available at: docs.nvidia.com/deploy/nvml-api/index.html
#
#   NVML_INCLUDE_DIRS - Path to header (nvml.h)
#   NVML_LIBRARIES    - Path to library (nvidia-ml)
#   NVML_FOUND        - True if both NVML_INCLUDE_DIRS & NVML_LIBRARIES are found

file(GLOB NVML_HEADER_HINT /usr/include/nvidia*/include
        /usr/local/cuda*/include /opt/cuda*/include)
find_path(NVML_INCLUDE_DIRS
        NAMES nvml.h
        PATH_SUFFIXES nvidia nvidia/include nvidia/gdk
        HINTS ${NVML_HEADER_HINT} ${CMAKE_HOME_DIRECTORY}/include
        DOC "Path to the NVML header (nvml.h)")

file(GLOB NVML_LIBRARIES_HINT /usr/lib*/nvidia* /opt/cuda*)
find_library(NVML_LIBRARIES
        NAMES libnvidia-ml nvidia-ml
        PATH_SUFFIXES nvidia nvidia/current nvidia/gdk
        HINTS ${NVML_LIBRARIES_HINT} ${CMAKE_HOME_DIRECTORY}/include
        DOC "Path to NVML static library (libnvidia-ml.so or nvidia-ml.so")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NVML
        REQUIRED_VARS NVML_INCLUDE_DIRS NVML_LIBRARIES
        FOUND_VAR NVML_FOUND)