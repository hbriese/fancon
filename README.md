# fancon

[![License](http://img.shields.io/badge/license-APACHE2-blue.svg)]()

fancon is a GNU/Linux fan control daemon and fan testing tool, allowing custom speed-temperature curves for fans, controllable by either PWM or RPM.
  - High performance
  - Low memory usage
  - High test coverage

Low overhead and easy configuration are the main goals of fancon, this is achieved by:
  - Use of C++, multi-threading, and optimized STL functions
  - Standard text file configuration - at /etc/fancon.conf
  - Fan characteristic testing - all fans are not equal, so testing enables RPM fan speed configuration, not just PWM control like similar tools
  - Support for stopping fans (for example, if not under load) and correct handling of required fan PWM for start


### Installation

###### Install fancon snap:

Snap [installation instructions](https://snapcraft.io/docs/core/install)

```sh
$ sudo snap install --beta --classic fancon
```

###### Build from source:
gcc may be substituted for clang

```sh
$ git clone https://github.com/HBriese/fancon.git
$ sudo apt-get install gcc make cmake lm-sensors rsyslog libc++-dev libstdc++6 linux-libc-dev libsensors4-dev libboost-filesystem-dev
$ mkdir fancon/Release && cd fancon/Release && cmake -DCMAKE_BUILD_TYPE=Release .. && make && sudo make install
```


### Runtime Dependencies

fancon aims to be widely deployable with minimum runtime dependencies:

| Name (package) | Usage |
| -------------- | ----- |
| [lm-sensors] | A sensor monitoring tool|
| syslog supported logger <br> (tested with [rsyslog]) | Log handling |


### Development

Want to contribute?
Pull requests, issues and feature requests are welcome.

Contributing developers please see TODO.md, or [email me](mailto:haydenbriese@gmail.com?subject=fancon). 

The code is formatted to the [LLVM style](http://clang.llvm.org/docs/ClangFormatStyleOptions.html) style using clang-format


### License

Copyright 2017 Hayden Briese

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


   [lm-sensors]: <https://wiki.archlinux.org/index.php/lm_sensors>
   [rsyslog]: http://www.rsyslog.com/
   [CMake]: https://cmake.org/
   [Boost-filesystem]: <http://www.boost.org/doc/libs/1_62_0/libs/filesystem/doc/index.htm>
   [pthread]: <https://www.gnu.org/software/hurd/libpthread.html>