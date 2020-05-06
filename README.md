# fancon

[![License](https://img.shields.io/github/license/hbriese/fancon)]()

A Linux user-space fan control daemon
  - %, RPM or PWM custom speed-temperature curve configuration
  - System, DELL and NVIDIA GPU fan control support


### Contents
- [Installation](#installation)     
- [Configuration](#configuration)
- [Usage](#usage)
- [Building from source](#building-from-source)
- [FAQ](#faq)


### Installation
[Download release package](https://github.com/hbriese/fancon/releases/latest)
OR [build from source](#building-from-source)

##### Ubuntu, Debian (.deb)
```bash
sudo dpkg -i ./fancon*.deb; sudo apt install -f
```

##### Fedora, Red Hat, CentOS (.rpm)
```bash
sudo yum –nogpgcheck install ./fancon*.rpm
```

### Configuration
***/etc/fancon.conf*** is automatically created on first run once the tests complete

**Only 'sensor' & 'temp_to_rpm' for each fan is required to be manually
 configured**

```text
config {
  update_interval: 1000             # Milliseconds between updating fan speeds
  dynamic: true                     # Interpolate speeds between temps e.g (30: 800, 50: 1200) results in 1000 @ 40 
  smoothing_intervals: 3            # Intervals over which to smooth RPM changes
  top_stickiness_intervals: 2       # Intervals to wait before decreasing RPM
  temp_averaging_intervals: 3       # Intervals to average temperatures over - eliminating temp spikes
}
devices {
  fan {
    type: SYS                       # One of: SYS (default), DELL, NVIDIA; may excluded if SYS
    label: "hwmon3/fan1"            # Name of device - anything you want
    sensor: "CPU Package"           # Sensor to read - specify by label
    temp_to_rpm: "39: 0%, 40: 1%, 75: 50%, 90: 100%"     
        # temp (optional f or F): RPM (optional % or PWM)
        #  40: 0%       Stopped at or below 40°C  (°C is used if F is omitted)
        #  50: 1%       Lowest running speed at 50°C
        #  75: 50%      50% of max RPM at 75°C, could also be written as 75: 180PWM in this case
        #  194f: 100%   Full speed at 90°C   (194f = 90°C)
    rpm_to_pwm: "0: 0, 3206: 128, 4954: 180, 7281: 255"    # Mappings of RPM to PWM
    start_pwm: 128                  # PWM at which the fan starts
    interval: 500                   # Milliseconds taken for the fan to reach a new speed, 
                                      → increasing improves testing accuracy but makes the test take longer
    ignore: false                   # Don't control or test device; may be excluded if false
    # Following only applicable to SYS & DELL devices
    driver_flag: 2                  # Driver flag to enable manual control
    pwm_path: "/sys/class/hwmon/hwmon3/pwm1"        # Path to read/write PWM
    rpm_path: "/sys/class/hwmon/hwmon3/fan1_input"  # Path to read RPM
  }
  fan {
    type: NVIDIA
    label: "980_Ti"
    sensor: "980_Ti_temp"
    temp_to_rpm: "54: 0%, 55: 1%, 90: 100%"
    rpm_to_pwm: "850: 0, 3832: 255"
    start_pwm: 128
    interval: 500
    id: 0                           # Only applicable to NVIDIA devices
  }
  sensor {
    type: SYS                   # One of: SYS (default), NVIDIA; may excluded if SYS
    label: "CPU Package"        # Name of device - anything you want
    input_path: "/sys/class/hwmon/hwmon5/temp1_input"   # Path to read (only applicable to SYS devices)
  }
  sensor {
    type: NVIDIA
    label: "980_Ti_temp"
    id: 0                          # Only applicable to NVIDIA devices 
  }
}
```

### Usage

```text
fancon argument=value ...
-h  help         Show this help
    start        Start the fan controller (default: true)
-s  stop         Stop the fan controller
-r  reload       Forces the fan controller to reload all data
-t  test         Force tests of all fans found
-ts test-safely  Test fans one at a time to avoid stopping all fans at once (default: false)
                   at once, avoiding high temps but significantly increasing duration
-c  config       Configuration file (default: /etc/fancon.conf)
-l  log-lvl      Sets the logging level: info, debug, trace, warning, error (default: info)
-d  daemonize    Daemonize the process (default: false)
-i  system-info  Save system info to the current directory
                   (file name default: fancon_system_info.txt)
```

#### FAQ

##### My fans aren't detected

First try detecting devices using lm-sensors
```bash
sudo sensors-detect
```
- You may need to reboot if unloaded kernel modules were added

Check to see if your device is detected
```bash
sensors
```

##### Fancon complains my device is mis-configured or unsupported

Devices (fans & sensors) that:
- Expose a sysfs like interface but are not found by lm-sensors
- Are reported as not having the required features but they do

**May** be configurable by [altering their configuration](#configuration) 

### Building from source:
##### With NVIDIA support:
```bash
sudo apt install clang cmake lm-sensors libsensors5 libsensors4-dev libboost-system-dev libboost-filesystem-dev libboost-log-dev libpthread-stubs0-dev libpstreams-dev libprotobuf-dev protobuf-compiler libgrpc++-dev protobuf-compiler protobuf-compiler-grpc libxnvctrl-dev libx11-dev

git clone https://github.com/hbriese/fancon.git && cd fancon
./gen_proto.sh; mkdir build; cd build
cmake -DCMAKE_BUILD_TYPE=Release -DNVIDIA_SUPPORT=ON .. && make -j && sudo make install
```

##### Without NVIDIA support:
```bash
sudo apt install clang cmake lm-sensors libsensors5 libsensors4-dev libboost-system-dev libboost-filesystem-dev libboost-log-dev libpthread-stubs0-dev libpstreams-dev libprotobuf-dev protobuf-compiler libgrpc++-dev protobuf-compiler protobuf-compiler-grpc

git clone https://github.com/hbriese/fancon.git && cd fancon
./gen_proto.sh; mkdir build; cd build
cmake -DCMAKE_BUILD_TYPE=Release -DNVIDIA_SUPPORT=OFF .. && make -j && sudo make install
```

| CMake Option     | Default | Description                                                                                 |
|:-----------------|:-------:| :-------------------------------------------------------------------------------------------|
| NVIDIA_SUPPORT   | ON      | Support for NVIDIA GPUs                                                                     |
| OPTIMIZE_DEBUG   | OFF     | Enable compiler optimizations on debug build                                                |
| PROFILE          | OFF     | Support for Google Perf Tools CPU & heap profilers; only recommended for debug builds       |
| LINT             | OFF     | Run lint checker (Clang-Tidy)                                                               |
