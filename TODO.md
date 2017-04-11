##TODO

- Arch package

### Bugs

- Get xauth file & xdisplay from files in /etc/fancon.d/

### Features

- Add support for AMD GPU fans
- Add options: new/exclude fan testing using regular expression option
- CLI fan point configuration graph
- Add 'precise' option for more accurate RPM control
- Time change per rpm (use stop_time?)
- Add display & xauth options to config file
- Check UID chip name against chip
- Better fail checking - exists(pwmX, rpmX, (?) pwmX_enable)
- Remove fan when control has been lost

### Performance 

- Test between "sed 's/^.*: //'" and "awk '{print $4}'"
- Better fan update search (starting from last used iterator) - requires storing last iterator

### Refactoring

- C++17: uncomment 'TODO: C++17' code
- C++17: nested namespaces

### Documentation

- Add performance comparison to fancontrol

## Watch

- NVML API - several functions missing before it can be used to replace XNVCtrl
- CMake 3.8 release