# fancon

[![License](https://img.shields.io/github/license/hbriese/fancon)]()

A Linux user-space fan control daemon
  - %, RPM or PWM custom speed-temperature curve configuration
  - System, DELL and NVIDIA GPU fan control support


### Contents
- [Installation](#installation)     
- [Building from source](#building-from-source)
- [Configuration](#configuration)
- [Usage](#usage)
- [Debugging issues](#debugging-issues)


### Installation

##### Ubuntu, Debian (.deb)
```bash
wget https://github.com/hbriese/fancon/releases/latest/download/fancon_amd64.deb
sudo dpkg -i ./fancon*.deb; sudo apt install -f
```

##### Fedora, Red Hat, CentOS (.rpm)
```bash
wget https://github.com/hbriese/fancon/releases/latest/download/fancon.x86_64.rpm
sudo yum –nogpgcheck install ./fancon*.rpm
```

##### Arch Linux (AUR)
```bash
git clone https://aur.archlinux.org/fancon.git; cd fancon
makepkg -sirc
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
        # temp (optional: f | F; default °C): RPM (optional: % | PWM; default RPM)
        #  40: 0%       Stopped at or below 40°C  (°C is used if F is omitted)
        #  50: 1%       Lowest running speed at 50°C
        #  75: 50%      50% of max RPM at 75°C, could also be written as 75: 180PWM in this case
        #  194f: 100%   Full speed at 194°F   (194f = 90°C)
    rpm_to_pwm: "0: 0, 3206: 128, 4954: 180, 7281: 255"    # Mappings of RPM to PWM
    start_pwm: 128                  # PWM at which the fan starts
    # interval: 500                 # Fan-specific update time; increasing improves test accuracy
    ignore: false                   # Don't control or test device; may be excluded if false
    # Following only applicable to SYS & DELL devices
    driver_flag: 2                  # Driver flag to enable manual control
    pwm_path: "/sys/class/hwmon/hwmon3/pwm1"            # Path to read/write PWM
    rpm_path: "/sys/class/hwmon/hwmon3/fan1_input"      # Path to read RPM
    # Following only applicable to SYS devices
    enable_path: "/sys/class/hwmon/hwmon3/pwm1_enable"  # Path to enable PWM control
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
fancon arg [value] ...
h  help           Show this help
s  status         Status of all fans
e  enable         Enable control of all fans
e  enable  [fan]  Enable control of the fan
d  disable        Disable control of all fans
d  disable [fan]  Disable control of the fans
t  test           Test all (untested) fans
t  test    [fan]  Test the fan if untested
f  force          Test even already tested fans (default: false)
r  reload         Reload config
c  config  [file] Config path (default: /etc/fancon.conf)
   service        Start as service
   daemon         Daemonize the process (default: false)
   stop-service   Stop the service
i  sysinfo [file] Save system info to file (default: sysinfo.txt)
   nv-init        Init nvidia devices
v  verbose        Debug logging level
a  trace          Trace logging level
```

### Debugging issues

##### Run in verbose mode: 

```fancon -v```

##### Reading logs

```journalctl -u fancon```

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

##### $XAUTHORITY and or $DISPLAY env variable(s) not set

fancon requires X11 access due to LibNVCtrl (NVIDIA control)

You will need to configure the unset environmental variable.

Manually configure the unset environmental variable(s)

Inside /etc/profile
- ```export XAUTHORITY=...```; You can find the XAuthority file by running ```xauth info```
- ```xhost si:localuser:root```; May be necessary on Wayland
 
https://wiki.archlinux.org/index.php/Running_GUI_applications_as_root 

### Building from source:
##### With NVIDIA support:
```bash
sudo apt install clang cmake lm-sensors libsensors5 libsensors4-dev libboost-system-dev libboost-filesystem-dev libboost-log-dev libpthread-stubs0-dev libpstreams-dev libprotobuf-dev protobuf-compiler libgrpc++-dev protobuf-compiler protobuf-compiler-grpc libxnvctrl-dev libx11-dev

git clone https://github.com/hbriese/fancon.git && cd fancon; mkdir build; cd build
cmake -DNVIDIA_SUPPORT=ON .. && make -j && sudo make install
```

##### Without NVIDIA support:
```bash
sudo apt install clang cmake lm-sensors libsensors5 libsensors4-dev libboost-system-dev libboost-filesystem-dev libboost-log-dev libpthread-stubs0-dev libpstreams-dev libprotobuf-dev protobuf-compiler libgrpc++-dev protobuf-compiler protobuf-compiler-grpc

git clone https://github.com/hbriese/fancon.git && cd fancon; mkdir build; cd build
cmake -DNVIDIA_SUPPORT=OFF .. && make -j && sudo make install
```

| CMake Option     | Default | Description                                        |
|:-----------------|:-------:| :--------------------------------------------------|
| NVIDIA_SUPPORT   | ON      | Support for NVIDIA GPUs                            |
| PROFILE          | OFF     | Support for Google Perf Tools CPU & heap profilers |
| LINT             | OFF     | Run Clang-Tidy                                     |
