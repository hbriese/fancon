cmake_minimum_required(VERSION 3.1)
set (CMAKE_CXX_STANDARD 14)
project(fancon)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wpedantic -Werror")
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")  ##  AND CMAKE_BUILD_TYPE STREQUAL "Release"
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
endif ()

# Enable testing in this project
#enable_testing()
#add_subdirectory(tests)
#add_test(fancon_tests tests/fancon_tests)

include_directories(src)
set(SOURCE_FILES src/main.cpp src/main.hpp src/Util.cpp src/Util.hpp src/UID.cpp src/UID.hpp src/SensorController.cpp
        src/SensorController.hpp src/Config.cpp src/Config.hpp src/Fan.cpp src/Fan.hpp src/NvidiaDevices.cpp
        src/NvidiaDevices.hpp src/FanInterface.cpp src/FanInterface.hpp src/SensorParentInterface.cpp src/SensorParentInterface.hpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

## Threads (pthread)
find_package(Threads REQUIRED)
set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

## Boost
add_definitions("-D BOOST_ALL_DYN_LINK")
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED COMPONENTS filesystem system log_setup log)
include_directories(${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})

## lm-sensors
find_path(LM_SENSORS_INCLUDE_DIRS NAMES sensors/sensors.h)
find_library(LM_SENSORS_LIBRARIES NAMES libsensors sensors)
include_directories(${LM_SENSORS_INCLUDE_DIRS})
set(LIBS ${LIBS} ${LM_SENSORS_LIBRARIES})

option(NVIDIA_SUPPORT "Build with NVIDIA GPU support" ON)

## X11
find_package(X11)
if (${X11_FOUND} AND NVIDIA_SUPPORT)
    include_directories(${X11_INCLUDE_DIR})
    set(LIBS ${LIBS} ${X11_LIBRARIES})

    # NVCTRLlib
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})
    find_package(NVCtrlLib)
    if (${NVCtrlLib_FOUND})
        include_directories(NVCtrlLib_INCLUDE_DIR)
        set(LIBS ${LIBS} ${NVCtrlLib_LIBRARY})
        add_definitions("-D FANCON_NVIDIA_SUPPORT")
        message("NVIDIA supported")
    endif () # NVCtrlLib_FOUND
endif () # X11_FOUND AND NVIDIA_SUPPORT

## Link libraries
target_link_libraries(${PROJECT_NAME} ${LIBS})

## Install
install(TARGETS ${PROJECT_NAME} DESTINATION bin)