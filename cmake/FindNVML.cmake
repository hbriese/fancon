# FindNVML
# --------
#
# Find the NVIDIA Management Library (NVML) includes and library. NVML documentation
# is available at: http://docs.nvidia.com/deploy/nvml-api/index.html
#
# NVML is part of the GPU Deployment Kit (GDK) and GPU_DEPLOYMENT_KIT_ROOT_DIR can
# be specified if the GPU Deployment Kit is not installed in a default location.
#
# FindNVML defines the following variables:
#
#   NVML_INCLUDE_DIR, where to find nvml.h, etc.
#   NVML_LIBRARY, the libraries needed to use NVML.
#   NVML_FOUND, If false, do not try to use NVML.

find_path(NVML_INCLUDE_DIR
        NAMES nvml.h
        DOC "Path to the NVML header (nvml.h)")

find_library(NVML_LIBRARY
        NAMES libnvidia-ml nvidia-ml
        PATH_SUFFIXES nvidia nvidia/current
        HINTS ${CMAKE_HOME_DIRECTORY}/include
        DOC "Path to NVML static library (libnvidia-ml.so or nvidia-ml.so")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NVML DEFAULT_MSG
        NVML_INCLUDE_DIR NVML_LIBRARY)