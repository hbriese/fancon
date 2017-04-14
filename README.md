# fancon

[![License](http://img.shields.io/badge/license-APACHE2-blue.svg)]()

A Linux fan control daemon and fan testing tool, allowing custom speed-temperature curves for fans, controllable by either PWM or RPM, or percentage.
  - High performance
  - Low memory usage
  - Support for system fans, and NVIDIA GPUs

Low overhead and easy, meaningful configuration are the main goals of fancon, this is achieved by:
  - Extensive multi-threading support
  - Use of C++ and performance profiling
  - Standard text file configuration - /etc/fancon.conf
  - Fan characteristic testing - allowing more meaningful speed configuration such as through fan RPM or percentage, not just PWM like similar tools
  - Speed percentage support (e.g. 65%) rather than cryptic fan-dependent PWM values (e.g. 142)
  - Fans may be turned off (for example, when system is not under load) with a guaranteed start when they are required


### Installation

Download latest appropriate release from github.com/hbriese/fancon/releases

###### Debian/Ubuntu

```sh
$ sudo dpkg -i ./fancon*.deb
```

###### Fedora
```sh
$ sudo yum –nogpgcheck install ./fancon*.rpm
```

##### Build from source:
Tested with both gcc & clang

```sh
$ sudo apt-get install gcc cmake libgcc-6-dev libc6-dev linux-libc-dev libc++-helpers lm-sensors libsensors4-dev libboost-system-dev libboost-filesystem-dev libboost-log-dev libpthread-stubs0-dev libpstreams-dev
$ git clone https://github.com/hbriese/fancon.git && cd fancon
$ mkdir build; cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j && sudo make install
```

| CMake Option     | Default | Description                                                                                 |
|:-----------------|:-------:| :-------------------------------------------------------------------------------------------|
| NVIDIA_SUPPORT   | ON      | Support for NVIDIA GPUs                                                                     |
| STATIC_LIBSTDC++ | OFF     | Statically link libstdc++ - useful for binary distribution                                  |
| OPTIMIZE_DEBUG   | OFF     | Enable compiler optimizations on debug build                                                |
| PROFILE          | OFF     | Support for Google Perf Tools CPU & heap profilers - for debug builds only, due to TCMalloc |
| LINT             | OFF     | Run lint checker (Clang-Tidy)                                                               |

##### Install from snap (currently not recommended):

Snap [installation instructions](https://snapcraft.io/docs/core/install)

###### stable
```sh
$ sudo snap install fancon
```

### Contributions

Want to contribute?
Pull requests, issues and feature requests are welcome.

Contributing developers please see TODO.md, or send an [email](mailto:haydenbriese@gmail.com?subject=fancon).

The code is formatted to the [LLVM style](http://clang.llvm.org/docs/ClangFormatStyleOptions.html) using clang-format.


### License

Copyright 2017 Hayden Briese

Licensed under the Apache License, Version 2.0 (the "License");
you may not use fancon except in compliance with the License.
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