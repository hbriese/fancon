# Find Sensors
# -----------
# Find (lm-sensors) Sensors library and headers
#
# lm-sensors can be found at hwmon.wiki.kernel.org (previously at www.lm-sensors.org)
# A clone is provided at github.com/groeck/lm-sensors
#
#   SENSORS_INCLUDE_DIR - Path to header (sensors.h)
#   SENSORS_LIBRARY     - Path to library (sensors)
#   SENSORS_FOUND       - Prue if both SENSORS_INCLUDE_DIR & SENSORS_LIBRARY are found

find_path(SENSORS_INCLUDE_DIR
        NAMES sensors.h
        PATH_SUFFIXES sensors)

find_library(SENSORS_LIBRARY
        NAMES sensors)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sensors DEFAULT_MSG
        SENSORS_INCLUDE_DIR SENSORS_LIBRARY)