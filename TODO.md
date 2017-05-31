##TODO

- Arch package

### Bugs

- Fan PWM writes fail once after reload - before correction
- Sensor thread may take > 100ms to update - causing old data to be used by associated fan

### Features

- Add support for AMD GPU fans
- Targeted fans option
- CLI fan point configuration graph
- qt configuration GUI
- Check UID chip name against chip
- Remove fan when control has been lost (maybe??)
- Add 'precise' option for more accurate RPM control
- Auto-config option - copy current controller configuration
- Check for NVIDIA hardware, and recommend libs for nvidia support
- C++17: template Option & custom config path (with tuple of options & std::apply)
- list-fans outputs manual mode (y/N) and current RPM
- Check fan is present while adding to the controller
- Don't rely on nvidia-xconfig - Screen _> Coolbits
- Set max width of '-----' to terminal width

### Performance

- Optimize X11 auth & display search
- Test between linear & binary search for points (of varies total sizes)
- C++17: replace appropriate `const string &`s with `string_view`
- Sort sensors by number of uses by fans for better locality

### Refactoring

- C++17: uncomment 'TODO C++17' code
- C++17: nested namespaces

### Documentation

- Add performance comparison to fancontrol

## Watch

- NVML: docs.nvidia.com/deploy/nvml-api/change-log.html#change-log for XNVCtrl functionality
- C++17: GCC 7 & Clang 4
