#include "Devices.hpp"

fc::SensorChips::SensorChips() {
  sensors_init(nullptr);

  const sensors_chip_name *cn = nullptr;
  for (int i = 0; (cn = sensors_get_detected_chips(nullptr, &i)) != nullptr;)
    chips.push_back(cn);
}

fc::SensorChips::~SensorChips() { sensors_cleanup(); }

void fc::SensorChips::enumerate(FanMap &fans, SensorMap &sensors) {
  for (const auto &sc : chips) {
    const path adapter_path(sc->path);

    const int buf_size = 200;
    char buf[buf_size];
    const string_view chip_name =
        (sensors_snprintf_chip_name(buf, buf_size, sc) > 0) ? buf : "";
    const bool is_dell = SMM::is_smm_dell(chip_name);

    const sensors_feature *sf;
    for (int fnum = 0; (sf = sensors_get_features(sc, &fnum)) != nullptr;) {
      const char *dev_name = sf->name, *dev_label = sensors_get_label(sc, sf);
      string label = adapter_path.filename().string() + "/" +
                     ((dev_label) ? dev_label : dev_name);

      // Only enumerate fans & sensors
      const bool is_fan = sf->type == SENSORS_FEATURE_FAN,
                 is_sensor = sf->type == SENSORS_FEATURE_TEMP;
      if (!is_fan && !is_sensor)
        continue;

      // Check subfeature is readable & compute mapping
      const auto input_ssf = (is_fan) ? SENSORS_SUBFEATURE_FAN_INPUT
                                      : SENSORS_SUBFEATURE_TEMP_INPUT;
      const sensors_subfeature *ssf = sensors_get_subfeature(sc, sf, input_ssf);
      const bool readable = ssf != nullptr &&
                            (ssf->flags & SENSORS_MODE_R) == SENSORS_MODE_R,
                 compute_mapping =
                     ssf != nullptr && (ssf->flags & SENSORS_COMPUTE_MAPPING) ==
                                           SENSORS_COMPUTE_MAPPING;

      if (!readable || !compute_mapping) {
        LOG(llvl::debug) << label << ": unable to read from device";
        continue;
      }

      if (is_fan) {
        const int id = Util::postfix_num(dev_name);
        unique_ptr<FanInterface> fan;
        if (is_dell) {
          fan = make_unique<FanDell>(label, adapter_path, id);
        } else {
          fan = make_unique<FanSysfs>(label, adapter_path, id);
        }

        if (fan->valid()) {
          fans.insert_or_assign(move(label), move(fan));
        } else {
          LOG(llvl::info) << *fan << ": mis-configured or unsupported";
        }
      } else { // is_sensor
        string dev_path = string(adapter_path.c_str()) + "/" + dev_name;
        unique_ptr<SensorInterface> sensor =
            make_unique<SensorSysfs>(label, move(dev_path));

        if (sensor->valid()) {
          sensors.insert_or_assign(move(label), move(sensor));
        } else {
          LOG(llvl::info) << *sensor << ": mis-configured or unsupported";
        }
      }
    }
  }
}

fc::Devices::Devices(bool dry_run) {
  SensorChips().enumerate(fans, sensors);

#ifdef FANCON_NVIDIA_SUPPORT
  fc::FanNV::enumerate(fans);
  fc::SensorNV::enumerate(sensors);
#endif // FANCON_NVIDIA_SUPPORT

  // Ignore all fans on dry run
  if (dry_run) {
    for (const auto &[key, f] : fans)
      f->ignore = true;
  }
}

void fc::Devices::from(const fc_pb::Devices &d) {
  std::set<string> uids;
  for (const fc_pb::Sensor &spb : d.sensor()) {
    shared_ptr<SensorInterface> s;

    switch (spb.type()) {
    case fc_pb::SYS:
    case fc_pb::DELL:
      s = make_shared<SensorSysfs>();
      break;
    case fc_pb::NVIDIA:
#ifdef FANCON_NVIDIA_SUPPORT
      if (!NV::xnvlib->supported) {
        LOG(llvl::warning) << "NVIDIA sensor configured but "
                              "NVIDIA control is disabled at this time";
        continue;
      }

      s = make_shared<SensorNV>();
#else
      LOG(llvl::warning) << "Skipping NVIDIA device, compile with flag: "
                         << "-DNVIDIA_SUPPORT=ON";
      continue;
#endif
      break;
    default:
      LOG(llvl::error) << "Skipping sensor, invalid type '" << spb.type()
                       << "' from " << spb.DebugString();
      continue;
    }

    s->from(spb);
    if (s->valid()) {
      const string uid = s->hw_id();
      if (uids.count(uid) == 0) {
        uids.emplace(uid);
        string label = s->label;
        sensors.emplace(move(label), move(s));
      } else {
        LOG(llvl::warning) << *s << ": skipping duplicate sensor in config";
      }
    } else {
      LOG(llvl::error) << "Skipping invalid sensor from config: " << *s;
    }
  }

  for (const fc_pb::Fan &fpb : d.fan()) {
    unique_ptr<FanInterface> f;

    switch (fpb.type()) {
    case fc_pb::SYS:
      f = make_unique<FanSysfs>();
      break;
    case fc_pb::DELL:
      f = make_unique<FanDell>();
      break;
    case fc_pb::NVIDIA:
#ifdef FANCON_NVIDIA_SUPPORT
      if (!NV::xnvlib->supported) {
        LOG(llvl::warning) << "NVIDIA fan is configured but "
                              "NVIDIA control is disabled at this time";
        continue;
      }

      f = make_unique<FanNV>();
#else
      LOG(llvl::warning) << "Skipping NVIDIA device, compile with flag: "
                         << "-DNVIDIA_SUPPORT=ON";
      continue;
#endif
      break;
    default:
      LOG(llvl::error) << "Skipping fan with invalid type (" << fpb.type()
                       << "): " << fpb.DebugString();
      continue;
    }

    f->from(fpb, sensors);
    if (f->valid()) {
      const string uid = f->hw_id(), label = f->label;
      if (uids.count(uid) == 0) {
        LOG(llvl::debug) << *f << ": imported";
        uids.emplace(uid);
        fans.insert_or_assign(label, move(f));
      } else {
        LOG(llvl::warning) << *f << ": skipping duplicate fan in config";
      }
    } else {
      LOG(llvl::warning) << *f << ": skipping invalid device from config";
    }
  }
}

void fc::Devices::to(fc_pb::Devices &d) const {
  for (const auto &[label, f] : fans)
    f->to(*d.mutable_fan()->Add());

  for (const auto &[label, s] : sensors)
    s->to(*d.mutable_sensor()->Add());
}

bool fc::operator==(const fc_pb::Fan &l, const fc_pb::Fan &r) {
  return l.type() == r.type() && l.pwm_path() == r.pwm_path() &&
         l.rpm_path() == r.rpm_path() && l.id() == r.id();
}

bool fc::operator==(const fc_pb::Sensor &l, const fc_pb::Sensor &r) {
  return l.type() == r.type() && l.input_path() == r.input_path() &&
         l.id() == r.id();
}
