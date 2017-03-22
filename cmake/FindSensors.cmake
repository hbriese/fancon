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

set(SENSORS_INCLUDE_DIR)
set(SENSORS_LIBRARY)
set(SENSORS_FOUND false)

find_path(SENSORS_INCLUDE_DIR NAMES sensors/sensors.h)
find_library(SENSORS_LIBRARY NAMES libsensors sensors)

if (SENSORS_INCLUDE_DIR AND SENSORS_LIBRARY)
    set(SENSORS_FOUND true)
endif () # SENSORS_INCLUDE_DIR AND SENSORS_LIBRARY