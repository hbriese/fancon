#include "SensorSysfs.hpp"

fc::SensorSysfs::SensorSysfs(string label_, const string &device_path)
    : fc::SensorInterface(move(label_)), input_path(device_path + "_input"),
      enable_path(if_exists(device_path + "_enable")),
      fault_path(if_exists(device_path + "_fault")),
      min_path(if_exists(device_path + "_min")),
      max_path(if_exists(device_path + "_max")),
      crit_path(if_exists(device_path + "_crit")) {
  if (is_faulty()) {
    LOG(llvl::warning) << *this << ": is faulty, ignoring";
    ignore = true;
  } else if (!enable()) {
    LOG(llvl::warning) << *this << ": failed to enable, ignoring";
    ignore = true;
  }
}

optional<Temp> fc::SensorSysfs::min_temp() const {
  const auto m = Util::read_<Temp>(min_path);
  if (!m)
    return nullopt;
  return *m / sysfs_temp_divisor;
}

optional<Temp> fc::SensorSysfs::max_temp() const {
  const auto m = Util::read_<Temp>(max_path), c = Util::read_<Temp>(crit_path);

  if (m && c) {
    return std::max(*m, *c) / sysfs_temp_divisor;
  } else if (m) {
    return *m / sysfs_temp_divisor;
  } else if (c) {
    return *c / sysfs_temp_divisor;
  } else {
    return nullopt;
  }
}

bool fc::SensorSysfs::valid() const { return exists(input_path); }

string_view fc::SensorSysfs::uid() const { return input_path.c_str(); }

void fc::SensorSysfs::from(const fc_pb::Sensor &s) {
  fc::SensorInterface::from(s);
  input_path = path(s.input_path());
  enable_path = path(s.enable_path());
  fault_path = path(s.fault_path());
  min_path = path(s.min_path());
  max_path = path(s.max_path());
  crit_path = path(s.crit_path());
}

void fc::SensorSysfs::to(fc_pb::Sensor &s) const {
  fc::SensorInterface::to(s);
  s.set_type(fc_pb::SYS);
  s.set_input_path(input_path.c_str());
  s.set_enable_path(enable_path.c_str());
  s.set_fault_path(fault_path.c_str());
  s.set_min_path(min_path.c_str());
  s.set_max_path(max_path.c_str());
  s.set_crit_path(crit_path.c_str());
}

Temp fc::SensorSysfs::read() const {
  return Util::read<Temp>(input_path) / sysfs_temp_divisor;
}

bool fc::SensorSysfs::enable() const {
  const auto enable_mode = Util::read_<control_flag_t>(enable_path);
  if (enable_mode && *enable_mode <= 0)
    return Util::write(enable_path, 1);

  return true;
}

bool fc::SensorSysfs::is_faulty() {
  return Util::read_<int>(fault_path).value_or(0) > 0;
}

path fc::SensorSysfs::if_exists(const path &p) { return (exists(p)) ? p : ""; }
