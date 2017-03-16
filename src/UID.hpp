#ifndef fancon_UID_HPP
#define fancon_UID_HPP

#include <algorithm>    // search, find, remove_if
#include <cctype>       // isspace
#include <exception>    // runtime_error
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

namespace fancon {
class UID {
public:
  UID(istream &is) {
    is >> *this;
    type = getType();
  }
  UID(string chipname, int hwID, string dev_name)
      : chipname(std::move(chipname)), hw_id(hwID), dev_name(std::move(dev_name)) { type = getType(); }

  DeviceType type;
  string chipname;
  int hw_id;
  string dev_name;

  /* FORMAT: e.g.
   * coretemp/0:temp1
   * it8620/1:fan2
   */

  bool valid() const;

  const string getBasePath() const;

  friend ostream &operator<<(ostream &os, const UID &u);
  friend istream &operator>>(istream &is, UID &u);

  bool operator==(const UID &other) const;

private:
  const char cn_esep = '/',
      hw_id_esep = ':';

  DeviceType getType();
};

ostream &operator<<(ostream &os, const fancon::UID &u);
istream &operator>>(istream &is, fancon::UID &u);
}

#endif //fancon_UID_HPP