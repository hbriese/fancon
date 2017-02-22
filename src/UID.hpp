#ifndef fancon_UID_HPP
#define fancon_UID_HPP

#include <algorithm>    // search, find, remove_if
#include <cctype>       // isspace
#include <exception>    // runtime_error
#include <iostream>     // skipws, cin
#include <string>
#include <sstream>      // ostream, istream
#include "Util.hpp"

using std::string;
using std::search;
using std::skipws;
using std::find;
using std::ostream;
using std::istream;
using std::to_string;
using fancon::Util::log;

namespace fancon {
class UID {
public:
  UID(istream &is) { is >> *this; }

  UID(const string &cn_prefix, int hwmon_id, const string feat_name)
      : cn_label(cn_prefix), hwmon_id(hwmon_id), dev_name(feat_name) {}

  string cn_label;
  int hwmon_id;
  string dev_name;

  /* FORMAT: e.g.
   * coretemp/0:temp1
   * it8620/1:fan2
   */

  bool valid() const { return (!cn_label.empty()) && (hwmon_id != -1) && (!dev_name.empty()); }

  const string getBasePath() const {
    string bpath(fancon::Util::hwmon_path);
    bpath.append(to_string(hwmon_id)).append("/").append(dev_name);
    return bpath;
  }

  friend ostream &operator<<(ostream &os, const UID &u) {
    os << u.cn_label << u.cn_end_sep << to_string(u.hwmon_id) << u.hwmon_id_end_sep << u.dev_name;
    return os;
  }

  friend istream &operator>>(istream &is, UID &u) {
    string in;
    is >> skipws >> in;
    std::remove_if(in.begin(), in.end(), [](auto &c) { return isspace(c); });

    auto cnBeginIt = in.begin();
    auto cnEndIt = find(cnBeginIt, in.end(), u.cn_end_sep);
    auto hwIdBegIt = cnEndIt + 1;
    auto hwIdEndIt = find(cnEndIt, in.end(), u.hwmon_id_end_sep);
    auto devnBegIt = hwIdEndIt + 1;
    auto devnEndIt = in.end();

    if (cnEndIt == in.end() || hwIdEndIt == in.end() || devnBegIt == in.end()) {
      if (!in.empty())
        log(LOG_ERR, string("Invalid UID: ") + in);

      // set invalid values
      u.cn_label = string();
      u.hwmon_id = -1;
      u.dev_name = string();
      return is;
    }

    u.cn_label = string(cnBeginIt, cnEndIt);
    string hwID = string(hwIdBegIt, hwIdEndIt);
    u.hwmon_id = stoi(hwID);
    u.dev_name = string(devnBegIt, devnEndIt);

    return is;
  }

  bool operator==(const UID &other) const {
    return (this->cn_label == other.cn_label) &&
        (this->dev_name == other.dev_name) &&
        (this->hwmon_id == other.hwmon_id);
  }

private:
  const char cn_end_sep = '/',
      hwmon_id_end_sep = ':';
};
}

#endif //fancon_UID_HPP