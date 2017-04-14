##TODO

- Arch package

### Bugs

- Fan PWM writes fail once after reload

### Features

- Add support for AMD GPU fans
- Add options: new/exclude fan testing using regular expression option
- CLI fan point configuration graph
- qt configuration
- Check UID chip name against chip
- Better fail checking - exists(pwmX, rpmX, (?) pwmX_enable)
- Remove fan when control has been lost (maybe??)
- Add 'precise' option for more accurate RPM control
- Auto-config option - copy current controller configuration
- Check for NVIDIA hardware, and recommend libs for nvidia support
- Update interval as a float

### Performance 

- Optimize X11 auth & display search
- Better fan update search (starting from last used iterator) - requires storing last iterator

### Refactoring

- C++17: uncomment 'TODO: C++17' code
- C++17: nested namespaces

### Documentation

- Add performance comparison to fancontrol

## Watch

- NVML: docs.nvidia.com/deploy/nvml-api/change-log.html#change-log for XNVCtrl functionality
- C++17: GCC 7 & Clang 4