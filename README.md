# fancon

[![License](http://img.shields.io/badge/license-APACHE2-blue.svg)]()

fancon is a C++ userspace fan control tool, allowing custom speed-temperature curve for each individual fan, controllable by either PWM or RPM.

  - High performance
  - Low memory usage

Low overhead and easy configuration are the main goals of fancon, this is achieved by:
  - Use of C++, multi-threading, and optimized STL functions
  - Fan characteristic testing - all fans are not equal, so testing enables RPM fan speed configuration, not just PWM control like similar tools
  - Text file configuration, great for headless machines - at /etc/fancon.conf
  - Allows stopping fans (for example, if not under load), with support for ensuring they start properly


### Installation

fancon is distributed as a snap package to allow for easy cross-distribution deployment:

###### Install snapd, for Debian/Ubuntu based distros:
```sh
$ sudo apt-get install snapd
```
###### Install fan``con snap:
```sh
$ sudo snap refresh && sudo snap install fancon
```

###### Build from source:

```sh
$ git clone https://github.com/HBriese/fancon.git fancon-src && cd fancon-src
$ sudo apt-get install lm-sensors rsyslog build-essentials make cmake libsensors4-dev libboost-filesystem-dev libexplain-dev
$ cmake . && make && sudo make install
```


### Runtime Dependencies

fancon aims to be widely deployable with minimum runtime dependencies:

| Name (package) | Usage |
| -------------- | ----- |
| [lm-sensors] | A sensor monitoring tool|
| [syslog] <br> (rsyslog) | Log handling |


### Development

Want to contribute? Pull requests, issues and feature requests are welcome.

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


   [dill]: <https://github.com/joemccann/dillinger>
   [git-repo-url]: <https://github.com/joemccann/dillinger.git>
   [john gruber]: <http://daringfireball.net>
   [df1]: <http://daringfireball.net/projects/markdown/>
   [markdown-it]: <https://github.com/markdown-it/markdown-it>
   [Ace Editor]: <http://ace.ajax.org>
   
   [lm-sensors]: <https://wiki.archlinux.org/index.php/lm_sensors>
   [syslog]: http://www.rsyslog.com/
   [CMake]: https://cmake.org/
   [Boost-filesystem]: <http://www.boost.org/doc/libs/1_62_0/libs/filesystem/doc/index.htm>
   [pthread]: <https://www.gnu.org/software/hurd/libpthread.html>
   [libexplain]: <http://libexplain.sourceforge.net/>
   [express]: <http://expressjs.com>
   [AngularJS]: <http://angularjs.org>
   [Gulp]: <http://gulpjs.com>

   [PlDb]: <https://github.com/joemccann/dillinger/tree/master/plugins/dropbox/README.md>
   [PlGh]: <https://github.com/joemccann/dillinger/tree/master/plugins/github/README.md>
   [PlGd]: <https://github.com/joemccann/dillinger/tree/master/plugins/googledrive/README.md>
   [PlOd]: <https://github.com/joemccann/dillinger/tree/master/plugins/onedrive/README.md>
   [PlMe]: <https://github.com/joemccann/dillinger/tree/master/plugins/medium/README.md>
   [PlGa]: <https://github.com/RahulHP/dillinger/blob/master/plugins/googleanalytics/README.md>