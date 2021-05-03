#include "SensorSysfs.hpp"

fc::SensorSysfs::SensorSysfs(string label_, const string &dev_path)
    : fc::Sensor(move(label_)), input_path(real_path(dev_path + "_input")),
      enable_path(real_path(dev_path + "_enable")), fault_path(real_path(dev_path + "_fault")),
      min_path(real_path(dev_path + "_min")), max_path(real_path(dev_path + "_max")),
      crit_path(real_path(dev_path + "_crit")) {
  if (is_faulty()) {
    LOG(llvl::warning) << *this << ": is faulty, ignoring";
    ignore = true;
  } else if (!enable()) {
    LOG(llvl::warning) << *this << ": failed to enable, ignoring";
    ignore = true;
  }
}

optional<Temp> fc::SensorSysfs::min_temp() const {
  const auto m = Util::read<Temp>(min_path);
  return (m) ? optional(*m / SYSFS_TEMP_DIVISOR) : nullopt;
}

optional<Temp> fc::SensorSysfs::max_temp() const {
  const auto m = Util::read<Temp>(max_path), c = Util::read<Temp>(crit_path);
  if (m && c) {
    return std::max(*m, *c) / SYSFS_TEMP_DIVISOR;
  } else if (m) {
    return *m / SYSFS_TEMP_DIVISOR;
  } else if (c) {
    return *c / SYSFS_TEMP_DIVISOR;
  } else {
    return nullopt;
  }
}

bool fc::SensorSysfs::valid() const {
  return input_path && exists(*input_path);
}

string fc::SensorSysfs::hw_id() const {
  return input_path->string();
}

void fc::SensorSysfs::from(const fc_pb::Sensor &s) {
  fc::Sensor::from(s);
  input_path = path(s.input_path());
  enable_path = path(s.enable_path());
  fault_path = path(s.fault_path());
  min_path = path(s.min_path());
  max_path = path(s.max_path());
  crit_path = path(s.crit_path());
}

void fc::SensorSysfs::to(fc_pb::Sensor &s) const {
  fc::Sensor::to(s);

  s.set_type(fc_pb::SYS);
  if (input_path)
    s.set_input_path(input_path->string());
  if (enable_path)
    s.set_enable_path(enable_path->string());
  if (fault_path)
    s.set_fault_path(fault_path->c_str());
  if (min_path)
    s.set_min_path(min_path->c_str());
  if (max_path)
    s.set_max_path(max_path->c_str());
  if (crit_path)
    s.set_crit_path(crit_path->c_str());
}

optional<Temp> fc::SensorSysfs::read() const {
  const auto temp = Util::read<Temp>(input_path);
  return (temp) ? optional(*temp / SYSFS_TEMP_DIVISOR) : nullopt;
}

bool fc::SensorSysfs::enable() const {
  const auto enable_mode = Util::read<control_flag_t>(enable_path);
  return !(enable_mode && *enable_mode <= 0) || Util::write(enable_path, 1);
}

bool fc::SensorSysfs::is_faulty() const {
  return Util::read<int>(fault_path).value_or(0) > 0;
}
