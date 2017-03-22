##TODO

### Bugs

### Features

- Add support for AMD GPU fans
- add options: new/exclude fan testing using regular expression option
- CLI fan point configuration graph
- Add 'precise' option for more accurate RPM control
- Make max threads limited by fans, rather than limited by temperature sensors (care race conditions)
- **Add support for PWM % point**
- time change per rpm (use stop_time?)
- Add display & xauth options to config file
- check UID chip name against chip
- better fail checking - exists(pwmX, rpmX, (?) pwmX_enable)

### Performance 

- test between "sed 's/^.*: //'" and "awk '{print $4}'"

### Refactoring

- Fan speed_change_t to chrono::seconds and replace sleep() with sleep_for()

### Documentation

- Add performance comparison to fancontrol

## Watch

NVML API - several functions missing before it can be used to replace XNVCtrl