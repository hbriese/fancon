#ifndef FANCON_UID_HPP
#define FANCON_UID_HPP

#include "Util.hpp"
#include <algorithm> // search, find, remove_if
#include <cctype>    // isspace
#include <exception> // runtime_error
#include <sstream>   // ostream, istream
#include <string>

using std::string;
using std::search;
using std::find;
using std::ostream;
using std::istream;
using std::to_string;

namespace fancon {
/// \note
/// Format: [chip_name/chip_id:device_name_&_number]
class UID {
public:
  UID(istream &is) : valid_(true) {
    is >> *this;
    type = getType();
  }
  UID(string chipname, int hwID, string dev_name)
      : chipname(std::move(chipname)), hw_id(hwID), dev_name(move(dev_name)),
        valid_(true) {
    type = getType();
  }

  DeviceType type;
  string chipname;
  int hw_id;
  string dev_name;
  bool valid_;

  bool isFan() const { return (type & DeviceType::fan_interface) == type; }
  bool isSensor() const {
    return (type & DeviceType::sensor_interface) == type;
  }
  bool valid(DeviceType type) const;

  const string getBasePath() const;

  friend ostream &operator<<(ostream &os, const UID &u);
  friend istream &operator>>(istream &is, UID &u);

  bool operator==(const UID &other) const;

private:
  DeviceType getType();
};

ostream &operator<<(ostream &os, const fancon::UID &u);
istream &operator>>(istream &is, fancon::UID &u);

namespace serialization_constants {
namespace uid {
const char cn_esep = '/', hw_id_esep = ':';
}
}
}

#endif // FANCON_UID_HPP
