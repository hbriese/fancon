#include "UID.hpp"

using namespace fancon;

bool UID::valid() const { return (!cn_label.empty()) && (hwmon_id != -1) && (!dev_name.empty()); }

const string UID::getBasePath() const {
  string bpath(fancon::Util::hwmon_path);
  bpath.append(to_string(hwmon_id)).append("/").append(dev_name);
  return bpath;
}

bool UID::operator==(const UID &other) const {
  return (this->cn_label == other.cn_label) &&
      (this->dev_name == other.dev_name) &&
      (this->hwmon_id == other.hwmon_id);
}

ostream &fancon::operator<<(ostream &os, const UID &u) {
  os << u.cn_label << u.cn_end_sep << to_string(u.hwmon_id) << u.hwmon_id_end_sep << u.dev_name;
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