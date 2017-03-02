##TODO

### Bugs

- check UID chip name against chip
- better fail checking - exists(pwmX, rpmX, (?) pwmX_enable)
- (NVIDIA) handle non-equal amount of GPUs and fans
- systemd service using wildly varying amounts of memory

### Features

- Add NV temperauter sensor as a source
- Add support for AMD GPU fans
- handle step up time and step down time (maybe?)
- add options: new/exclude fan testing using regular expression option
- CLI fan point configuration graph
- Add 'precise' option for more accurate RPM control
- Make max threads limited by fans, rather than limited by temperature sensors (care race conditions)
- Run sudo sensors-detect on install, then fancon test
- Automatically set CoolBits option
- Add support for PWM % point
- time change per rpm (use stop_t?)

### Performance 

- test between | sed 's/^.*: //' and | awk '{print $4}'

### Refactoring

- don't 'REQUIRE' X11, but set compile option

### Documentation

- Add performance comparison to fancontrol