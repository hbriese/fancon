cmake_minimum_required(VERSION 3.3)
set(CMAKE_CXX_STANDARD 14)  # TODO: C++17 support in CMake 3.8 (also cmake_minimum_required -> 3.8)
project(fancon)
set(PACKAGE_AUTHOR "Hayden Briese <haydenbriese@gmail.com>")

## Additional cmake modules provided in ./cmake/
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

## Optimization flag - lvl 2
set(OPTIMIZATION_FLAG "-O2")

## Debug flags
option(OPTIMIZE_DEBUG "Optimize debug build - useful for profiling" OFF)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wpedantic -Werror")
    if (OPTIMIZE_DEBUG)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${OPTIMIZATION_FLAG}")
    endif ()
endif ()

## Release flags
if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${OPTIMIZATION_FLAG}")
endif ()

## Option for statically linking libstdc++ for distributions which provide older versions
option(STATIC_LIBSTDC++ "Statically link libstdc++ when building with g++" OFF)
if (STATIC_LIBSTDC++) # AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
    message("Statically linking libstdc++")
endif () # STATIC_LIBSTDC++

include_directories(src)
set(SOURCE_FILES src/main.cpp src/main.hpp src/Util.cpp src/Util.hpp src/Logging.hpp src/Logging.cpp
        src/UID.cpp src/UID.hpp src/Find.cpp src/Find.hpp src/Config.cpp src/Config.hpp
        src/Fan.cpp src/Fan.hpp src/FanInterface.cpp src/FanInterface.hpp src/SensorInterface.cpp src/SensorInterface.hpp
        src/NvidiaDevices.cpp src/NvidiaDevices.hpp src/NvidiaUtil.cpp src/NvidiaUtil.hpp src/Controller.cpp src/Controller.hpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

## Threads (pthread)
find_package(Threads REQUIRED)
set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

## Boost
add_definitions("-DBoost_USE_MULTITHREADED")
add_definitions("-DBOOST_ALL_DYN_LINK")
find_package(Boost REQUIRED COMPONENTS thread system filesystem log)
include_directories(${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})

## Sensors (lm-sensors)
find_package(Sensors REQUIRED)
include_directories(${SENSORS_INCLUDE_DIR})
set(LIBS ${LIBS} ${SENSORS_LIBRARY})

## NVIDIA Support
option(NVIDIA_SUPPORT "Build with NVIDIA GPU support" ON)
if (NVIDIA_SUPPORT)
    find_package(X11)
    find_package(NVCtrlLib)

    # ONLY X11 and XNVCtrl headers are required, libs are dynamically loaded
    if (X11_INCLUDE_DIR AND NVCtrlLib_INCLUDE_DIR)
        include_directories(${X11_INCLUDE_DIR} ${NVCtrlLib_INCLUDE_DIR})
        set(LIBS ${LIBS} ${CMAKE_DL_LIBS})
        add_definitions("-DFANCON_NVIDIA_SUPPORT")
        message("NVIDIA support enabled")
    else ()
        message("NVIDIA_SUPPORT=${NVIDIA_SUPPORT} but X11 and or NVCtrl headers weren't found!")
    endif ()
endif () # NVIDIA_SUPPORT

## NVML
option(NVML_SUPPORT "Build with experimental NVML support" OFF)
if (NVML_SUPPORT)
    find_package(NVML REQUIRED)
    set(LIBS ${LIBS} ${NVML_LIBRARY})
    message("Experimental NVML support enabled")
    add_definitions("-DFANCON_NVML_SUPPORT_EXPERIMENTAL")
endif () # NVML_SUPPORT

# TCMalloc - for heap profiling; ONLY available for debug builds as TCMalloc currently does not release memory
option(TCMALLOC "Build binary using TCMalloc for all dynamic memory allocation - ONLY available for debug builds" OFF)
find_package(TCMalloc)
if (TCMALLOC_FOUND AND ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(LIBS ${LIBS} ${TCMALLOC_LIBRARIES})
    add_definitions("-DHEAP_PROFILE")
endif () # TCMALLOC_FOUND AND ${CMAKE_BUILD_TYPE} STREQUAL "Debug"

## Link libraries
target_link_libraries(${PROJECT_NAME} ${LIBS})

## Run lint if debug build/
option(LINT "Run linter with debug build" OFF)
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND LINT)
    set(LINT_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++${CMAKE_CXX_STANDARD}")
    set(LINT_CHECKS "*,-clang-diagnostic-unused-command-line-argument,\
    -*readability-braces-around-statements,-google-runtime-int")
    set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_CLANG_TIDY "clang-tidy"
            "-checks=${LINT_CHECKS} -- ${LINT_CXX_FLAGS}")
endif () # ${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND LINT

## Install
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin)