cmake_minimum_required(VERSION 3.3)
project(fancon LANGUAGES CXX)
set(PACKAGE_AUTHOR "Hayden Briese <haydenbriese@gmail.com>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## Clang must be used because of linking with Boost Threads
set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

## Additional cmake modules provided in ./cmake/
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

## Default to Release if not set
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

message("Build type: ${CMAKE_BUILD_TYPE}")

## Release flags
set(OPTIMIZATION_FLAG "-O3")
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${OPTIMIZATION_FLAG}")
endif ()

## Debug flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wpedantic")
endif ()

## Generate protobuf source files
execute_process(COMMAND "./gen_proto.sh" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GENPROTO_RES OUTPUT_VARIABLE GENPROTO_OUT ERROR_VARIABLE GENPROTO_OUT)
if (${GENPROTO_RES} STREQUAL "0")
    message("Protobuf files generated ${GENPROTO_OUT}")
else ()
    message("Failed to generate protobuf files: ${GENPROTO_OUT}")
endif ()

set(SRC "${PROJECT_SOURCE_DIR}/src")
set(PSRC "${SRC}/proto")
include_directories(src)
set(SOURCE_FILES ${SRC}/main.cpp ${SRC}/main.hpp
        ${SRC}/util/Util.cpp ${SRC}/util/Util.hpp
        ${SRC}/util/Logging.hpp ${SRC}/util/Logging.cpp
        ${PSRC}/DevicesSpec.pb.cc ${PSRC}/DevicesSpec.pb.h
        ${PSRC}/DevicesSpec.grpc.pb.cc ${PSRC}/DevicesSpec.grpc.pb.h
        ${SRC}/Service.cpp ${SRC}/Service.hpp
        ${SRC}/util/Args.hpp ${SRC}/util/Args.cpp
        ${SRC}/Client.cpp ${SRC}/Client.hpp
        ${SRC}/Controller.cpp ${SRC}/Controller.hpp
        ${SRC}/fan/FanTask.cpp ${SRC}/fan/FanTask.hpp
        ${SRC}/Devices.cpp ${SRC}/Devices.hpp
        ${SRC}/fan/Fan.cpp ${SRC}/fan/Fan.hpp
        ${SRC}/sensor/Sensor.cpp ${SRC}/sensor/Sensor.hpp
        ${SRC}/fan/FanSysfs.cpp ${SRC}/fan/FanSysfs.hpp
        ${SRC}/sensor/SensorSysfs.cpp ${SRC}/sensor/SensorSysfs.hpp
        ${SRC}/nvidia/NvidiaUtil.cpp ${SRC}/nvidia/NvidiaUtil.hpp
        ${SRC}/nvidia/NvidiaDevices.cpp ${SRC}/nvidia/NvidiaDevices.hpp
        ${SRC}/nvidia/NvidiaNvml.cpp ${SRC}/nvidia/NvidiaNvml.hpp
        ${SRC}/dell/DellSmm.cpp ${SRC}/dell/DellSmm.hpp
        ${SRC}/dell/FanDell.cpp ${SRC}/dell/FanDell.hpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

## Threads (pthread)
find_package(Threads REQUIRED)
set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

## Boost
add_definitions("-DBoost_USE_MULTITHREADED")
add_definitions("-DBOOST_ALL_DYN_LINK")
find_package(Boost REQUIRED COMPONENTS log thread system filesystem)
include_directories(${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})

## Sensors (lm-sensors)
find_package(Sensors REQUIRED)
include_directories(${SENSORS_INCLUDE_DIR})
set(LIBS ${LIBS} ${SENSORS_LIBRARY})

## NVIDIA Support
option(NVIDIA_SUPPORT "Support for NVIDIA GPUs" ON)
if (NVIDIA_SUPPORT)
    find_package(X11)
    find_package(NVCtrl)

    # ONLY X11 and XNVCtrl headers are required, libs are dynamically loaded
    if (X11_INCLUDE_DIR AND NVCTRL_INCLUDE_DIR)
        include_directories(${X11_INCLUDE_DIR} ${NVCTRL_INCLUDE_DIR})
        set(LIBS ${LIBS} ${CMAKE_DL_LIBS})
        add_definitions("-DFANCON_NVIDIA_SUPPORT")
        message("NVIDIA support enabled")
    else ()
        if (NOT X11_INCLUDE_DIR)
            message("NVIDIA_SUPPORT enabled but X11lib headers weren't found!")
        endif ()
        if (NOT NVCTRL_INCLUDE_DIR)
            message("NVIDIA_SUPPORT enabled but NVCtrl headers weren't found!")
        endif ()
    endif ()
endif ()

## NVML - need to wait for device set speed support to replace XNVCtrl
option(NVML_SUPPORT "Build with experimental NVML support" OFF)
if (NVML_SUPPORT)
    find_package(NVML REQUIRED)
    include_directories(${NVML_INCLUDE_DIRS})
    set(LIBS ${LIBS} ${NVML_LIBRARIES})
    add_definitions("-DFANCON_NVIDIA_NVML_SUPPORT")
    message("Experimental NVML support enabled")
endif ()

## ProtoBuff
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Protobuf_LIBRARIES})

# gRPC
find_package(GRPC REQUIRED)
include_directories(${GRPC_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GRPC_LIBRARIES})

## Google Perf Tools profiling
option(PROFILE "Support for Google Perf Tools CPU & heap profilers" OFF)
if (PROFILE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_package(Profiler REQUIRED)
    find_package(TCMalloc REQUIRED)
    set(LIBS ${LIBS} ${PROFILER_LIBRARY} ${TCMALLOC_LIBRARIES})
    add_definitions("-DFANCON_PROFILE")
    message("Profiling enabled, warning: TCMalloc doesn't release memory!")
endif ()

## Link libraries
target_link_libraries(${PROJECT_NAME} ${LIBS})

## Handle GNU CMAKE_INSTALL variables
if (CMAKE_INSTALL_SYSCONFDIR)
    message("Setting sysconfdir to ${CMAKE_INSTALL_SYSCONFDIR}")
    message("Full sysnconfdir: ${CMAKE_INSTALL_FULL_SYSCONFDIR}")
    add_definitions("-DFANCON_SYSCONFDIR=\"${CMAKE_INSTALL_SYSCONFDIR}\"")
endif ()
if (CMAKE_INSTALL_LOCALSTATEDIR)
    add_definitions("-DFANCON_LOCALSTATEDIR=\"${CMAKE_INSTALL_LOCALSTATEDIR}\"")
endif ()

set(FANCON_BINDIR "bin")
if (CMAKE_INSTALL_BINDIR)
    set(FANCON_BINDIR ${CMAKE_INSTALL_BINDIR})
endif ()

## Run lint if debug build
option(LINT "Run Clang-Tidy" OFF)
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND LINT)
    set(LINT_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++${CMAKE_CXX_STANDARD}")
    set(LINT_CHECKS "*,-clang-diagnostic-unused-command-line-argument,\
    -*readability-braces-around-statements,-google-runtime-int")
    set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_CLANG_TIDY "clang-tidy"
            "-checks=${LINT_CHECKS} -- ${LINT_CXX_FLAGS}")
endif ()

## Install
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${FANCON_BINDIR})
#install(FILES ${PROJECT_SOURCE_DIR}/debian/fancon.man.1
#        DESTINATION share/man/man1)
