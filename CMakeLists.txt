cmake_minimum_required(VERSION 3.2.2)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
project(fancon)

# Enable testing in this project
#enable_testing()
#add_subdirectory(tests)
#add_test(test1 tests/fancond_tests)

include_directories(src)
set(SOURCE_FILES src/main.cpp src/main.hpp src/Util.cpp src/Util.hpp src/UID.hpp src/SensorController.cpp src/SensorController.hpp
        src/Config.cpp src/Config.hpp src/TemperatureSensor.cpp src/TemperatureSensor.hpp src/Fan.cpp src/Fan.hpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

## Threads
find_package(Threads REQUIRED)
set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

## Boost
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED COMPONENTS filesystem system)   #thread
include_directories(${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})

## lm-sensors
find_path(LM_SENSORS_INCLUDE_DIRS NAMES sensors/sensors.h)
find_library(LM_SENSORS_LIBRARIES NAMES libsensors sensors)
include_directories(${LM_SENSORS_INCLUDE_DIRS})
set(LIBS ${LIBS} ${LM_SENSORS_LIBRARIES})

## Link libraries
target_link_libraries(${PROJECT_NAME} ${LIBS})

install(TARGETS ${PROJECT_NAME} DESTINATION bin)