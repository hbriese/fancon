#include "UID.hpp"

using namespace fancon;

bool UID::valid() const { return (!chipname.empty()) && (hwID != -1) && (!dev_name.empty()); }

const string UID::getBasePath() const {
  string bpath(fancon::Util::hwmon_path);
  bpath.append(to_string(hwID)).append("/").append(dev_name);
  return bpath;
}

bool UID::operator==(const UID &other) const {
  return (this->chipname == other.chipname) &&
      (this->dev_name == other.dev_name) &&
      (this->hwID == other.hwID);
}

DeviceType UID::getType() {
  const string tempDevName("temp");

  if (chipname == Util::nvidia_label)
    return DeviceType::FAN_NVIDIA;
  else if (search(dev_name.begin(), dev_name.end(), tempDevName.begin(), tempDevName.end()) != dev_name.end())
    return DeviceType::TEMP_SENSOR;
  else
    return DeviceType::FAN;
}

ostream &fancon::operator<<(ostream &os, const UID &u) {
  os << u.chipname << u.cn_end_sep << to_string(u.hwID) << u.hwmon_id_end_sep << u.dev_name;
  return os;
}

istream &fancon::operator>>(istream &is, UID &u) {
  string in;
  is >> in;
  std::remove_if(in.begin(), in.end(), [](auto &c) { return isspace(c); });

  auto cnBeginIt = in.begin();
  auto cnEndIt = find(cnBeginIt, in.end(), u.cn_end_sep);
  auto hwIdBegIt = cnEndIt + 1;
  auto hwIdEndIt = find(cnEndIt, in.end(), u.hwmon_id_end_sep);
  auto devnBegIt = hwIdEndIt + 1;
  auto devnEndIt = in.end();

  if (cnEndIt == in.end() || hwIdEndIt == in.end() || devnBegIt == in.end()) {
    if (!in.empty())
      LOG(severity_level::error) << "Invalid UID: " << in;

    // set invalid values
    u.chipname = string();
    u.hwID = -1;
    u.dev_name = string();
    return is;
  }

  u.chipname = string(cnBeginIt, cnEndIt);
  string hwID = string(hwIdBegIt, hwIdEndIt);
  u.hwID = stoi(hwID);
  u.dev_name = string(devnBegIt, devnEndIt);

  return is;
}