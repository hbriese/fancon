#include "Util.hpp"

optional<string> fc::Util::read_line(const path &p, bool failed) {
  std::ifstream ifs(p.string());
  if (!ifs)
    return nullopt;

  string line;
  std::getline(ifs, line);
  ifs.close();

  if (!ifs) {
    if (const bool fexists = exists(p); fexists && !failed)
      return read_line(p, true);

    return nullopt;
  }

  return line;
}

string fc::Util::join(std::initializer_list<pair<bool, string>> args, string join_with) {
  vector<decltype(args.begin())> to_join;
  for (auto it = args.begin(); it != args.end(); ++it) {
    if (it->first)
      to_join.push_back(it);
  }

  std::stringstream ss;
  for (size_t i = 0; i < to_join.size(); ++i) {
    ss << to_join.at(i)->second;
    if (i < to_join.size() - 1)
      ss << join_with;
  }

  return ss.str();
}

bool fc::Util::is_root() { return getuid() == 0; }

bool fc::Util::is_atty() { return isatty(STDOUT_FILENO); }

std::chrono::high_resolution_clock::time_point fc::Util::deadline(long ms) {
  return std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(ms);
}

bool fc::Util::deep_equal(const google::protobuf::Message &m1, const google::protobuf::Message &m2) {
  return m1.ByteSizeLong() == m2.ByteSizeLong() && m1.SerializeAsString() == m2.SerializeAsString();
}

optional<path> fc::Util::real_path(path p) {
  p = absolute(p);
  char buf[PATH_MAX];
  if (!exists(p) || realpath(p.c_str(), &buf[0]) == nullptr)
    return nullopt;

  return path(buf);
}

fc::Util::ScopedCounter<atomic_int> fc::Util::RemovableMutex::acquire_lock() {
  while (counter < 0)
    sleep_for(milliseconds(50));
  return ScopedCounter(counter);
}

fc::Util::ScopedCounter<atomic_int> fc::Util::RemovableMutex::acquire_removal_lock() {
  while (counter > 0)
    sleep_for(milliseconds(100));
  return ScopedCounter(counter, false);
}
