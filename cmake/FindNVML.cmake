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

set(NVML_INCLUDE_DIR)
set(NVML_LIBRARY)
set(NVML_FOUND false)

find_path(NVML_INCLUDE_DIR NAMES nvml.h)

set(NV_LIB_DIR "/usr/lib/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux-gnu")
find_library(NVML_LIBRARY NAMES nvidia-ml libnvidia-ml HINTS
        ${NV_LIB_DIR}/nvidia ${NV_LIB_DIR}/nvidia/current ${CMAKE_HOME_DIRECTORY}/include)

if (NVML_LIBRARY AND NVML_INCLUDE_DIR)
    set(NVML_FOUND true)
endif ()