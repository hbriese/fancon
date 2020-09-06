#include "FanInterface.hpp"

fc::FanInterface::FanInterface(string label_) : label(move(label_)) {}

void fc::FanInterface::update() {
  const auto set = [&](Rpm rpm) {
    rpm = smooth_rpm(rpm);
    const Pwm target = find_closest_pwm(rpm);
    set_pwm(target);

    sleep_for_interval();

    // Recover control if the PWM changes (after sleeping) from the target
    //    if (get_pwm() != target) {
    //      LOG(llvl::debug) << *this << ": mismatch (t, a) = (" << target
    //                       << ", " << get_pwm() << ")";
    //      return recover_control();
    //    }
  };

  const Temp temp = sensor->get_average_temp();

  // Lower bound is >=; Upper bound is >
  auto floor_it = temp_to_rpm.lower_bound(temp); // Floor now >= temp

  // temp >= max temp; set to the highest RPM
  if (floor_it == temp_to_rpm.end())
    return set(next(floor_it, -1)->second);

  // temp <= min temp || temp == floor temp; set to the lowest RPM
  if (floor_it == temp_to_rpm.begin() || floor_it->first == temp)
    return set(floor_it->second);

  // min temp < temp < max temp
  if (floor_it->first > temp) // Make floor <= temp
    --floor_it;

  // Static; set to the closest RPM <= temp
  if (!fc::dynamic)
    return set(floor_it->second);

  // Dynamic: find the RPM between the floor & ceiling
  const auto ceil_it = next(floor_it); // ceil > target
  const Rpm rpm_range = ceil_it->second - floor_it->second;

  const double temp_range_weight =
      static_cast<double>(temp) / (floor_it->first + ceil_it->first);
  const Rpm dynamic_rpm =
      floor_it->second + std::floor(temp_range_weight * rpm_range);

  set(dynamic_rpm);
}

bool fc::FanInterface::tested() const {
  return (PWM_MIN <= start_pwm && start_pwm <= PWM_MAX) && !rpm_to_pwm.empty();
}

bool fc::FanInterface::pre_start_check() const {
  if (ignore) {
    LOG(llvl::debug) << *this << ": ignored";
  } else if (const bool ce = temp_to_rpm.empty(), se = !sensor,
             si = (sensor && sensor->ignore);
             ce || se || si) {
    LOG(llvl::warning) << *this << ": skipping - "
                       << Util::join({{ce, "curve not configured"},
                                      {se, "sensor not configured"},
                                      {si, "sensor ignored"}});
  } else if (!enable_control()) {
    LOG(llvl::error) << *this << ": failed to enable";
  } else {
    return true;
  }

  return false;
}

Pwm fc::FanInterface::find_closest_pwm(Rpm rpm) {
  // Find RPM closest to rpm
  const auto ge_it = rpm_to_pwm.lower_bound(rpm); // >= rpm
  if (ge_it == rpm_to_pwm.begin())                // rpm < the min point
    return ge_it->second;

  const auto le_it = next(ge_it, -1); // <= rpm
  Pwm pwm;
  if (ge_it != rpm_to_pwm.end()) {
    // Choose the closer of two points
    pwm = ((rpm - le_it->first) <= (ge_it->first - rpm)) ? le_it->second
                                                         : ge_it->second;
  } else {
    pwm = le_it->second;
  }

  const bool needs_starting = pwm > 0 && pwm < start_pwm && get_rpm() == 0;
  return (needs_starting) ? start_pwm : pwm;
}

bool fc::FanInterface::recover_control() const {
  for (auto i = 1; i <= 5; ++i, sleep_for_interval())
    if (enable_control()) {
      LOG(llvl::debug) << *this << ": recovering control";
      return true;
    }

  LOG(llvl::warning) << *this << ": lost control";
  return false;
}

Rpm fc::FanInterface::smooth_rpm(const Rpm rpm) {
  const Rpm rpm_delta = rpm - smoothing.targeted_rpm;
  if (rpm_delta == 0)
    return rpm;

  // Don't try smoothing when the fan just started
  if (smoothing.just_started) {
    smoothing.just_started = false;
    smoothing.rem_intervals = fc::smoothing_intervals;
    smoothing.top_stickiness_rem_intervals = fc::top_stickiness_intervals;
    smoothing.targeted_rpm = rpm;
    return rpm;
  }

  // Be top sticky when a rpm decrease is requested
  if (rpm_delta < 0) {
    if (smoothing.top_stickiness_rem_intervals > 0) {
      smoothing.top_stickiness_rem_intervals--;
      return smoothing.targeted_rpm;
    }
  } else { // Reset stickiness when RPM increases (delta > 0)
    smoothing.top_stickiness_rem_intervals = fc::top_stickiness_intervals;
  }

  // Don't bother smoothing for minor RPM changes
  if (rpm_delta < (0.1 * rpm_to_pwm.rbegin()->first)) {
    smoothing.targeted_rpm = rpm;
    return rpm;
  }

  // Restart smoothing on rpm target change
  if (rpm != smoothing.targeted_rpm || smoothing.targeted_rpm <= 0)
    smoothing.rem_intervals = fc::smoothing_intervals;

  smoothing.targeted_rpm += rpm_delta / smoothing.rem_intervals--;
  return smoothing.targeted_rpm;
}

void fc::FanInterface::sleep_for_interval() const {
  sleep_for((interval.count() > 0) ? interval : fc::update_interval);
}

void fc::FanInterface::test(ObservableNumber<int> &status) {
  const Pwm pre_pwm = get_pwm();

  // Fail early if can't write enable mode or pwm
  const bool ec = enable_control(), spt = set_pwm_test();
  if (!ec || !spt) {
    LOG(llvl::error) << *this << ": failed to take control";
    disable_control();
    status += 100;
    return;
  }

  // 100 (%) should be added to status over the series of tests
  status = 0;
  Pwm_to_Rpm_Map pwm_to_rpm;

  test_stopped(pwm_to_rpm);
  status += 10;
  test_start(pwm_to_rpm);
  status += 30;
  test_running_min(pwm_to_rpm);
  status += 30;
  test_mapping(pwm_to_rpm);
  status += 30;

  rpm_to_pwm_from(pwm_to_rpm);

  // Restore pre-test Rpm & driver control
  set_pwm(pre_pwm);
  disable_control();
}

optional<Rpm> fc::FanInterface::set_stabilised_pwm(const Pwm pwm) const {
  if (!set_pwm(pwm))
    return nullopt;

  // Rpm must not increase more than 5% twice consecutively to be 'stable'
  Rpm cur = get_rpm(), prev = 0;
  uint reached = 0;
  while (reached <= 4) {
    sleep_for_interval();
    if (get_pwm() != pwm)
      return nullopt;

    prev = cur;
    cur = get_rpm();
    reached = ((cur - prev) <= (0.05 * cur)) ? reached + 1 : 0;
  }

  return cur;
}

bool fc::FanInterface::set_pwm_test() const {
  for (int i = 0; i < 3; ++i) {
    const Pwm target = (get_pwm() != PWM_MIN) ? PWM_MIN : PWM_MAX;
    if (set_stabilised_pwm(target).has_value())
      return true;
  }
  return false;
}

void fc::FanInterface::test_stopped(Pwm_to_Rpm_Map &pwm_to_rpm) {
  set_stabilised_pwm(PWM_MIN);
  pwm_to_rpm[get_pwm()] = get_rpm(); // Ideally RPM will now be 0
}

void fc::FanInterface::test_start(Pwm_to_Rpm_Map &pwm_to_rpm) {
  Pwm target_pwm = fc::PWM_MIN;
  optional<Rpm> cur_rpm = 0;
  while ((!cur_rpm || *cur_rpm == 0) && target_pwm <= fc::PWM_MAX) {
    cur_rpm = set_stabilised_pwm(target_pwm);
    target_pwm += 2;
  }

  // Driver may have altered the PWM from that set, also be conservative
  target_pwm = min(target_pwm + 6, PWM_MAX);
  sleep_for_interval();
  start_pwm = get_pwm();
  pwm_to_rpm[start_pwm] = *cur_rpm;
}

void fc::FanInterface::test_interval(Pwm_to_Rpm_Map &pwm_to_rpm) {
  if (interval > milliseconds(0)) // Use user's set interval if configured
    return;

  set_stabilised_pwm(PWM_MIN);

  // Method 1: time taken from min pwm (stopped) to start RPM
  // Method 2: if fan can't stop - time taken from min pwm to max pwm
  const auto start =
      (!pwm_to_rpm.empty()) ? next(pwm_to_rpm.begin()) : pwm_to_rpm.end();
  const Pwm target_pwm = (start_pwm != PWM_MIN && start != pwm_to_rpm.end())
                             ? start->first
                             : PWM_MAX;

  const auto pre = chrono::high_resolution_clock::now();
  if (!set_stabilised_pwm(target_pwm))
    return;

  interval = chrono::duration_cast<milliseconds>(
      chrono::high_resolution_clock::now() - pre);
}

void fc::FanInterface::test_running_min(Pwm_to_Rpm_Map &pwm_to_rpm) {
  // Slowly drop PWM until fan is no longer running
  // Don't take the last 3 results before it stopped to be safe
  vector<tuple<Pwm, Rpm>> results;
  Pwm target_pwm = start_pwm;

  for (optional<Rpm> cur_rpm; target_pwm >= PWM_MIN; target_pwm -= 2) {
    cur_rpm = set_stabilised_pwm(target_pwm);
    if (!cur_rpm)
      continue;

    if (*cur_rpm <= 0)
      break;

    results.emplace_back(target_pwm, *cur_rpm);
  }

  // Save all but the last 2 results
  const ulong last_i = (results.size() > 2) ? results.size() - 2 : 0;
  for (ulong i = 0; i < last_i; ++i) {
    const auto [pwm, rpm] = results[i];
    pwm_to_rpm[pwm] = rpm;
  }
}

void fc::FanInterface::test_mapping(Pwm_to_Rpm_Map &pwm_to_rpm) {
  // Record points from start PWM to PWM_MAX
  // Ensure target hits 128 (even) && 255 (odd)
  Pwm target = min(start_pwm + ((start_pwm % 2 != 0) ? 1 : 2), PWM_MAX);
  for (; target <= PWM_MAX; target += (target < PWM_MAX - 1) ? 2 : 1) {
    if (const auto cur_rpm = set_stabilised_pwm(target); cur_rpm)
      pwm_to_rpm[target] = *cur_rpm;
  }
}

Percent fc::FanInterface::rpm_to_percent(const Rpm rpm) const {
  if (rpm <= 0)
    return 0;

  const auto max_it = rpm_to_pwm.rbegin();
  auto min_running_it = rpm_to_pwm.begin();
  if (min_running_it->first == 0 && rpm_to_pwm.size() > 1)
    ++min_running_it;

  // 1% is the min running RPM
  const Rpm adjusted = rpm - min_running_it->first,
            range = max_it->first - min_running_it->first;
  return (adjusted > 0)
             ? std::min(static_cast<int>(std::ceil(adjusted / range)), 100)
             : 1;
}

Rpm fc::FanInterface::percent_to_rpm(Percent percent) const {
  if (percent <= 0)
    return 0;

  if (percent > 100) {
    LOG(llvl::warning) << *this << " temp_to_rpm: invalid value " << percent
                       << "%, setting to max (100%)";
    percent = 100;
  }

  if (rpm_to_pwm.empty())
    throw runtime_error("RPM to PWM can't be empty!");

  const auto max_it = rpm_to_pwm.rbegin();
  auto min_running_it = rpm_to_pwm.begin();
  if (min_running_it->first == 0 && rpm_to_pwm.size() > 1)
    ++min_running_it;

  // 1% is the min running RPM
  const Rpm range = max_it->first - min_running_it->first;
  return (percent == 1) ? min_running_it->first
                        : ((percent / 100) * range) + min_running_it->first;
}

Rpm fc::FanInterface::pwm_to_rpm(Pwm pwm) const {
  // Retrieve the closest RPM match for the given pwm
  if (rpm_to_pwm.empty())
    throw runtime_error("RPM to PWM can't be empty!");

  if (pwm < PWM_MIN || pwm > PWM_MAX) {
    LOG(llvl::warning) << *this << " temp_to_rpm: " << pwm
                       << " PWM is invalid, valid values are between 0-255";
    pwm = clamp_pwm(pwm);
  }

  auto it = rpm_to_pwm.begin();
  for (; it != rpm_to_pwm.end(); ++it) {
    if (it->second == pwm)
      return it->first;

    if (it->second > pwm) {
      const auto last = prev(it);
      const bool it_closer = last == rpm_to_pwm.begin() ||
                             (it->second - pwm) < (pwm - last->second);
      return (it_closer) ? it->first : last->first;
    }
  }

  return (--it)->first;
}

void fc::FanInterface::temp_to_rpm_from(const string &src) {
  if (rpm_to_pwm.empty()) // Fan needs to be tested first
    return;

  const optional<Temp> min_temp = sensor->min_temp(),
                       max_temp = sensor->max_temp();

  string::const_iterator start_it = src.begin(), next_it = src.end();
  std::smatch m;

  // 1: temp, 2: is_fahrenheit, 3: rpm, 4: is_percent, 5: is_pwm
  const std::regex reg(R"((\d+)\s*([fF])?[cC]?\s*[:]\s*(\d+)\s*(%)?(PWM)?)");
  while (std::regex_search(start_it, next_it, m, reg)) {
    const bool is_fahrenheit = m[2].matched, is_percent = m[4].matched,
               is_pwm = m[5].matched;

    auto temp = stoi(m[1]);
    if (is_fahrenheit)
      temp = (5.0 / 9) * (temp - 32);

    auto rpm = stoi(m[3]);
    if (is_percent)
      rpm = percent_to_rpm(rpm);
    else if (is_pwm)
      rpm = pwm_to_rpm(rpm);

    temp_to_rpm[temp] = rpm;

    if (min_temp && temp < *min_temp) {
      LOG(llvl::warning) << *this << ": " << temp << "°C < sensor min ("
                         << *min_temp << "°C)";
    } else if (max_temp && temp > *max_temp) {
      LOG(llvl::warning) << *this << ": " << temp << "°C > sensor max ("
                         << *max_temp << "°C)";
    }

    // The next value starts after the match ends
    next_it = m[0].second;
    start_it = (next_it != src.end()) ? next(next_it) : next_it;
  }
}

void fc::FanInterface::rpm_to_pwm_from(const string &src) {
  string::const_iterator start_it = src.begin(), next_it = src.end();
  std::smatch m;

  // 1: rpm, 2: pwm
  const std::regex reg(R"((\d+)\s*:\s*(\d{1,3}))");
  while (std::regex_search(start_it, next_it, m, reg)) {
    rpm_to_pwm[Util::stou(m[1])] = clamp_pwm(Util::stou(m[2]));

    // The next value starts after the match ends
    next_it = m[0].second;
    start_it = (next_it != src.end()) ? next(next_it) : next_it;
  }
}

void fc::FanInterface::rpm_to_pwm_from(const Pwm_to_Rpm_Map &pwm_to_rpm) {
  // Put pwm & rpm values into the rpm_to_pwm map
  // Ensure that RPM only increases with PWM!
  vector<Rpm> rpms;
  rpms.reserve(pwm_to_rpm.size());
  for (const auto p : pwm_to_rpm)
    rpms.emplace_back(p.second);
  std::sort(rpms.begin(), rpms.end());

  rpm_to_pwm.clear();
  auto pwm_it = pwm_to_rpm.begin();
  for (auto rpm_it = rpms.begin();
       pwm_it != pwm_to_rpm.end() && rpm_it != rpms.end(); ++pwm_it, ++rpm_it) {
    rpm_to_pwm[*rpm_it] = clamp_pwm(pwm_it->first);
  }
}

void fc::FanInterface::from(const fc_pb::Fan &f, const SensorMap &sensor_map) {
  label = f.label();
  if (const auto s_it = sensor_map.find(f.sensor()); s_it != sensor_map.end())
    sensor = s_it->second;

  rpm_to_pwm_from(f.rpm_to_pwm());
  temp_to_rpm_from(f.temp_to_rpm());
  start_pwm = clamp_pwm(f.start_pwm());
  interval = milliseconds(f.interval());
  ignore = f.ignore();
}

void fc::FanInterface::to(fc_pb::Fan &f) const {
  f.set_label(label);
  f.set_sensor(sensor ? sensor->label : "");
  f.set_rpm_to_pwm(Util::map_str(rpm_to_pwm));
  f.set_temp_to_rpm(Util::map_str(temp_to_rpm));
  f.set_start_pwm(start_pwm);
  f.set_interval(interval.count());
  f.set_ignore(ignore);
}

bool fc::FanInterface::deep_equal(const FanInterface &other) const {
  fc_pb::Fan f, fother;
  to(f);
  other.to(fother);
  return Util::deep_equal(f, fother);
}

std::ostream &fc::operator<<(std::ostream &os, const fc::FanInterface &f) {
  os << f.label;
  return os;
}

Pwm fc::clamp_pwm(Pwm pwm) { return std::clamp(pwm, PWM_MIN, PWM_MAX); }
