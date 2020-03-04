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

std::ostream &fc::Util::amp(std::ostream &os) {
  if (os.tellp() != os.beg)
    os << " & ";
  return os;
}
