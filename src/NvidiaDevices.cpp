#ifdef FANCON_NVIDIA_SUPPORT

#include "NvidiaDevices.hpp"

using fc::NV::xnvlib;

fc::FanNV::FanNV(string label, NVID id) : FanInterface(move(label)), id(id) {}

fc::FanNV::~FanNV() {
  if (!ignore)
    FanNV::disable_control();
}

void fc::FanNV::from(const fc_pb::Fan &f, const SensorMap &sensor_map) {
  fc::FanInterface::from(f, sensor_map);
  id = f.id();
}

void fc::FanNV::to(fc_pb::Fan &f) const {
  fc::FanInterface::to(f);
  f.set_type(fc_pb::NVIDIA);
  f.set_id(id);
}

bool fc::FanNV::valid() const {
  return xnvlib->pwm_percent.read(id).has_value();
}

string fc::FanNV::uid() const { return string("NV:f") + to_string(id); }

vector<unique_ptr<fc::FanInterface>> fc::FanNV::enumerate() {
  vector<unique_ptr<fc::FanInterface>> fans;

  if (!NV::xnvlib)
    NV::xnvlib = make_unique<NV::LibXNvCtrl>();

  int n_gpus = NV::xnvlib->get_num_GPUs();
  for (int i = 0; i < n_gpus; ++i) {
    unsigned char *buf = nullptr;
    int len{}; // Buffer size allocated by NVCTRL
    int ret = NV::xnvlib->QueryTargetBinaryData(
        *NV::xnvlib->xdisplay, NV_CTRL_TARGET_TYPE_GPU, i, 0,
        NV_CTRL_BINARY_DATA_COOLERS_USED_BY_GPU, &buf, &len);
    if (!ret || buf == nullptr) {
      LOG(llvl::error) << "Failed to query number of fans for GPU: " << i;
      continue;
    }

    const vector<NVID> fan_ids = NV::LibXNvCtrl::from_binary_data(buf, len);
    for (const auto &id : fan_ids) {
      const string id_str = (fan_ids.size() > 1) ? to_string(id) : "";
      fans.emplace_back(make_unique<fc::FanNV>(
          NV::xnvlib->get_gpu_product_name(i) + id_str, id));
    }
  }

  return fans;
}

Rpm fc::FanNV::get_rpm() const {
  const auto rpm = xnvlib->rpm.read(id);
  if (!rpm)
    LOG(llvl::error) << *this << ": failed to get rpm";
  return rpm.value_or(0);
}

Pwm fc::FanNV::get_pwm() const {
  const auto pwm_pc = xnvlib->pwm_percent.read(id);
  if (!pwm_pc)
    LOG(llvl::error) << *this << ": failed to get pwm";
  return percent_to_pwm(pwm_pc.value_or(0));
}

bool fc::FanNV::set_pwm(const Pwm pwm) const {
  // Attempt to recover control of the device if the write fails
  if (!xnvlib->pwm_percent.write(id, pwm_to_percent(pwm)))
    return FanInterface::recover_control();

  return true;
}

bool fc::FanNV::enable_control() const {
  return xnvlib->enable_mode.write(id, NV_CTRL_GPU_COOLER_MANUAL_CONTROL_TRUE);
}

bool fc::FanNV::disable_control() const {
  return xnvlib->enable_mode.write(id, NV_CTRL_GPU_COOLER_MANUAL_CONTROL_FALSE);
}

Percent fc::FanNV::pwm_to_percent(const Pwm pwm) {
  return static_cast<Percent>((static_cast<double>(pwm) / pwm_max_abs) * 100);
}

Pwm fc::FanNV::percent_to_pwm(const Percent percent) {
  return static_cast<Pwm>((static_cast<double>(percent) / 100) * pwm_max_abs);
}

fc::SensorNV::SensorNV(string label, NVID id)
    : SensorInterface(move(label)), id(id) {}

Temp fc::SensorNV::read() const {
  const auto temp = xnvlib->temp.read(id);
  if (!temp)
    LOG(llvl::error) << *this << ": failed to get temp";
  return temp.value_or(0);
}

void fc::SensorNV::from(const fc_pb::Sensor &s) {
  fc::SensorInterface::from(s);
  id = s.id();
}

void fc::SensorNV::to(fc_pb::Sensor &s) const {
  fc::SensorInterface::to(s);
  s.set_type(fc_pb::NVIDIA);
  s.set_id(id);
}

bool fc::SensorNV::valid() const { return xnvlib->temp.read(id).has_value(); }

string fc::SensorNV::uid() const { return string("NV:s") + to_string(id); }

SensorMap fc::SensorNV::enumerate() {
  SensorMap sensor_map;

  if (!NV::xnvlib)
    NV::xnvlib = make_unique<NV::LibXNvCtrl>();

  int n_gpus = NV::xnvlib->get_num_GPUs();
  for (int i = 0; i < n_gpus; ++i) {
    int len{};
    unsigned char *buf = nullptr;
    int ret = NV::xnvlib->QueryTargetBinaryData(
        *NV::xnvlib->xdisplay, NV_CTRL_TARGET_TYPE_GPU, i, 0,
        NV_CTRL_BINARY_DATA_THERMAL_SENSORS_USED_BY_GPU, &buf, &len);

    if (!ret || buf == nullptr) {
      LOG(llvl::error)
          << "Failed to query number of temperature sensors for GPU: " << i;
      continue;
    }

    const vector<NVID> sensor_ids = NV::LibXNvCtrl::from_binary_data(buf, len);
    for (const auto &id : sensor_ids) {
      const string id_str = (sensor_ids.size() > 1) ? to_string(id) : "";
      const string l = NV::xnvlib->get_gpu_product_name(i) + id_str + "_temp";
      sensor_map.emplace(l, make_unique<fc::SensorNV>(l, id));
    }
  }

  return sensor_map;
}

#endif // FANCON_NVIDIA_SUPPORT
