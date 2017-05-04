#include "Devices.hpp"

using fancon::Devices;

unique_ptr<FanInterface> Devices::getFan(const UID &uid, const Config &fanConf,
                                         bool dynamic) {
  assert(uid.isFan());

  if (uid.type == DeviceType::fan)
    return make_unique<Fan>(uid, fanConf, dynamic);
  else
    return make_unique<FanNV>(uid, fanConf, dynamic);
}

vector<UID> Devices::getFanUIDs() {
  auto fans = getHWMonUIDs(FanCharacteristicPaths::rpm_prefix);

#ifdef FANCON_NVIDIA_SUPPORT
  auto nvFans = NV::getFans();
  if (!nvFans.empty())
    Util::moveAppend(nvFans, fans);
#endif // FANCON_NVIDIA_SUPPORT

  fans.shrink_to_fit();
  return fans;
}

unique_ptr<SensorInterface> Devices::getSensor(const UID &uid) {
  assert(uid.isSensor());

  if (uid.type == DeviceType::sensor)
    return make_unique<Sensor>(uid);
  else
    return make_unique<SensorNV>(uid);
}

vector<UID> Devices::getSensorUIDs() {
  auto sensors = getHWMonUIDs(Util::temp_sensor_label);

#ifdef FANCON_NVIDIA_SUPPORT
  auto nvSensors = NV::getSensors();
  if (!nvSensors.empty())
    Util::moveAppend(nvSensors, sensors);
#endif // FANCON_NVIDIA_SUPPORT

  sensors.shrink_to_fit();
  return sensors;
}

vector<UID> Devices::getHWMonUIDs(const char *devicePathSuffix) {
  vector<UID> uids;

  SensorsWrapper sensors;
  for (const auto &sc : sensors.chips) {
    auto hwmon_id = lastNum(string(sc->path));

    const sensors_feature *sf;
    for (int fi{0}; (sf = sensors_get_features(sc, &fi)) != nullptr;) {
      // Check for supported device postfix, e.g. fan or temp
      string name(sf->name);
      if (name.find(devicePathSuffix) != string::npos)
        uids.emplace_back(fancon::UID(sc->prefix, hwmon_id, name));
    }
  }

  return uids;
}

SensorsWrapper::SensorsWrapper() {
  sensors_init(nullptr);

  const sensors_chip_name *cn = nullptr;
  for (int i{0}; (cn = sensors_get_detected_chips(nullptr, &i)) != nullptr;)
    chips.push_back(cn);
}
