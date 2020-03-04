#ifndef FANCON_UTIL_HPP
#define FANCON_UTIL_HPP

#include <algorithm>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <charconv>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <sys/ioctl.h>
#include <tuple>
#include <utility>
#include <vector>
//#include <chrono>
//#include <chrono>

#include "Logging.hpp"

namespace fs = std::filesystem;
namespace chrono = boost::chrono;

using boost::this_thread::sleep_for;
using chrono::milliseconds;
using chrono::seconds;
using fs::exists;
using fs::path;
using std::cout;
using std::endl;
using std::from_chars;
using std::make_shared;
using std::make_tuple;
using std::make_unique;
using std::map;
using std::move;
using std::next;
using std::nullopt;
using std::optional;
using std::prev;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::tie;
using std::to_chars;
using std::to_string;
using std::tuple;
using std::unique_ptr;
using std::vector;

namespace fc::Util {
int postfix_num(const string_view &s);
optional<string> read_line(const path &fpath, bool failed = false);
template <typename T> T read(const path &fpath, bool failed = false);
template <typename T> optional<T> read_(const path &fpath, bool failed = false);
template <typename T> bool write(const path &fpath, T val, bool failed = false);
template <typename K, typename T> string map_str(const std::map<K, T> m);
std::ostream &amp(std::ostream &os);
} // namespace fc::Util

//----------------------//
// TEMPLATE DEFINITIONS //
//----------------------//

template <typename T> T fc::Util::read(const path &fpath, bool failed) {
  const auto fatal_err = [&fpath](bool fexists) -> T {
    LOG(llvl::debug) << "Failed to read from: " << fpath << " - "
                     << ((fexists) ? "filesystem error" : "doesn't exist");
    return T{};
  };

  std::ifstream ifs(fpath.string());
  if (!ifs)
    return fatal_err(exists(fpath));

  T ret;
  ifs >> ret;

  ifs.close();

  if (!ifs) {
    const bool fexists = exists(fpath);
    if (fexists && !failed)
      return read<T>(fpath, true);

    return fatal_err(fexists);
  }

  return ret;
}

template <typename T> T from(std::istream &is) {
  T ret;
  is >> ret;
  return ret;
}

template <typename T>
optional<T> fc::Util::read_(const path &fpath, bool failed) {
  const auto retry = [&fpath, &failed] {
    return (!failed && exists(fpath)) ? read_<T>(fpath, true) : std::nullopt;
  };

  std::ifstream ifs(fpath.string());
  if (!ifs)
    return retry();

  auto ret = from<T>(ifs);
  ifs.close();

  if (!ifs)
    return retry();

  return ret;
}

template <typename T>
bool fc::Util::write(const path &fpath, T val, bool failed) {
  std::ofstream ofs(fpath.string());
  if (!ofs) {
    const char *msg = exists(fpath) ? "can't open: " : "doesn't exist: ";
    LOG(llvl::error) << "Failed to read file, " << msg << fpath;
    return false;
  }

  ofs << val;
  ofs.close();

  if (!ofs) {
    if (!failed)
      return write(fpath, move(val), true);

    LOG(llvl::debug) << "Failed to write '" << val << "' to: " << fpath;
    return false;
  }

  return true;
}

template <typename K, typename T>
string fc::Util::map_str(const std::map<K, T> m) {
  std::stringstream ss;
  for (auto it = m.begin(); it != m.end();) {
    ss << it->first << ": " << it->second;
    if (++it != m.end())
      ss << ", ";
  }
  return ss.str();
}

#endif // FANCON_UTIL_HPP
