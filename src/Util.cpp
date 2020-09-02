#include "Util.hpp"

int fc::Util::postfix_num(const string_view &s) {
  bool numFound = false;
  auto begRevIt = std::find_if_not(s.rbegin(), s.rend(), [&](const char &c) {
    return (std::isdigit(c)) ? (numFound = true) : !numFound;
  });

  int num;
  const auto res = std::from_chars(begRevIt.base(), s.data() + s.size(), num);
  return (res.ec == std::errc()) ? num : -1;
}

optional<string> fc::Util::read_line(const path &fpath, bool failed) {
  std::ifstream ifs(fpath.string());
  if (!ifs)
    return std::nullopt;

  string line;
  std::getline(ifs, line);
  ifs.close();

  if (!ifs) {
    if (const bool fexists = exists(fpath); fexists && !failed)
      return read_line(fpath, true);

    return std::nullopt;
  }

  return line;
}

string fc::Util::join(std::initializer_list<pair<bool, string>> args,
                      string join_with) {
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
  return std::chrono::high_resolution_clock::now() +
         std::chrono::milliseconds(ms);
}

bool fc::Util::deep_equal(const google::protobuf::Message &m1,
                          const google::protobuf::Message &m2) {
  return m1.ByteSizeLong() == m2.ByteSizeLong() &&
         m1.SerializeAsString() == m2.SerializeAsString();
}

fc::Util::ScopedCounter<atomic_int> fc::Util::RemovableMutex::acquire_lock() {
  while (counter < 0)
    sleep_for(milliseconds(50));
  return ScopedCounter(counter);
}

fc::Util::ScopedCounter<atomic_int>
fc::Util::RemovableMutex::acquire_removal_lock() {
  while (counter > 0)
    sleep_for(milliseconds(100));
  return ScopedCounter(counter, false);
}
