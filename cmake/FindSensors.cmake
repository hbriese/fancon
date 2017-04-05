# FindSensors
# -----------
#
# Find the Sensors (lm-sensors) library (libsensors) and headers
# lm-sensors can be found at hwmon.wiki.kernel.org (previously at www.lm-sensors.org)
# A clone is provided at github.com/groeck/lm-sensors
#
# FindSensors declares the following variables:
#
#   SENSORS_INCLUDE_DIR, where to find sensors.h
#   SENSORS_LIBRARY, the library needed to use sensors
#   SENSORS_FOUND, returns true if both the header and library is found

find_path(SENSORS_INCLUDE_DIR
        NAMES sensors.h
        PATH_SUFFIXES sensors)

find_library(SENSORS_LIBRARY
        NAMES libsensors sensors)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sensors DEFAULT_MSG
        SENSORS_INCLUDE_DIR SENSORS_LIBRARY)