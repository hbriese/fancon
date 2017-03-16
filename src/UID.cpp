#include "UID.hpp"

using namespace fancon;

bool UID::valid() const { return (!chipname.empty()) && (hw_id != -1) && (!dev_name.empty()); }

const string UID::getBasePath() const {
  string bpath(fancon::Util::hwmon_path);
  bpath.append(to_string(hw_id)).append("/").append(dev_name);
  return bpath;
}

bool UID::operator==(const UID &other) const {
  return (this->chipname == other.chipname) &&
      (this->dev_name == other.dev_name) &&
      (this->hw_id == other.hw_id);
}

DeviceType UID::getType() {
  const string sdn(Util::temp_sensor_label);
  const bool isSensor = search(dev_name.begin(), dev_name.end(), sdn.begin(), sdn.end()) != dev_name.end();
  const bool isNVIDIA = chipname == Util::nvidia_label;

  DeviceType type;

  if (isNVIDIA)
    type = (isSensor) ? DeviceType::SENSOR_NVIDIA : DeviceType::FAN_NVIDIA;
  else
    type = (isSensor) ? DeviceType::SENSOR : DeviceType::FAN;

  return type;
}

ostream &fancon::operator<<(ostream &os, const UID &u) {
  os << u.chipname << u.cn_esep << to_string(u.hw_id) << u.hw_id_esep << u.dev_name;
  return os;
}

istream &fancon::operator>>(istream &is, UID &u) {
  string in;
  is >> in;
  std::remove_if(in.begin(), in.end(), [](auto &c) { return isspace(c); });

  auto cnBeginIt = in.begin();
  auto cnEndIt = find(cnBeginIt, in.end(), u.cn_esep);
  auto hwIdBegIt = cnEndIt + 1;
  auto hwIdEndIt = find(cnEndIt, in.end(), u.hw_id_esep);
  auto devnBegIt = hwIdEndIt + 1;
  auto devnEndIt = in.end();

  if (cnEndIt == in.end() || hwIdEndIt == in.end() || devnBegIt == in.end()) {
    if (!in.empty())
      LOG(llvl::error) << "Invalid UID: " << in;

    // set invalid values
    u.chipname = string();
    u.hw_id = -1;
    u.dev_name = string();
    return is;
  }

  u.chipname = string(cnBeginIt, cnEndIt);
  string hwID = string(hwIdBegIt, hwIdEndIt);
  u.hw_id = stoi(hwID);
  u.dev_name = string(devnBegIt, devnEndIt);

  return is;
}