#ifndef FANCON_UTIL_HPP
#define FANCON_UTIL_HPP

#include <algorithm>
#include <atomic>
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
#include <tuple>
#include <utility>
#include <vector>
//#include <chrono>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <google/protobuf/message.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "Logging.hpp"

namespace fs = std::filesystem;
namespace chrono = boost::chrono;

using boost::this_thread::sleep_for;
using chrono::milliseconds;
using chrono::seconds;
using fs::exists;
using fs::path;
using std::atomic_int;
using std::cout;
using std::endl;
using std::from_chars;
using std::function;
using std::lock_guard;
using std::make_shared;
using std::make_tuple;
using std::make_unique;
using std::map;
using std::move;
using std::mutex;
using std::next;
using std::nullopt;
using std::optional;
using std::pair;
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
static const string SERVICE_ADDR = "0.0.0.0:5820";

int postfix_num(const string_view &s);
optional<string> read_line(const path &fpath, bool failed = false);
template <typename T> T read(const path &fpath, bool failed = false);
template <typename T> optional<T> read_(const path &fpath, bool failed = false);
template <typename T> bool write(const path &fpath, T val, bool failed = false);
template <typename K, typename T> string map_str(std::map<K, T> m);
string join(std::initializer_list<pair<bool, string>> args,
            string join_with = " & ");
bool is_root();
bool is_atty();
std::chrono::high_resolution_clock::time_point deadline(long ms);
bool deep_equal(const google::protobuf::Message &m1,
                const google::protobuf::Message &m2);

template <class T> class ObservableNumber {
public:
  explicit ObservableNumber(T &&value) : value(value) {}
  ObservableNumber(function<void(T &)> f, T &&value = 0) : value(value) {
    register_observer(f);
  }

  vector<function<void(T &)>> observers;

  void register_observer(function<void(T &)> callback);
  void notify_observers();

  ObservableNumber<T> &operator=(T other);
  ObservableNumber<T> &operator+=(const T &other);

private:
  T value;
  mutex update_mutex;
};

template <class T> class ScopedCounter {
public:
  explicit ScopedCounter(T &counter, bool increment = true)
      : counter(counter), increment(increment) {
    counter += (increment ? 1 : -1);
  }
  ~ScopedCounter() { counter += (increment ? -1 : 1); }

private:
  T &counter;
  const bool increment;
};

class RemovableMutex {
public:
  ScopedCounter<atomic_int> acquire_lock();
  ScopedCounter<atomic_int> acquire_removal_lock();

private:
  atomic_int counter = atomic_int(0);
};
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

template <class T>
void fc::Util::ObservableNumber<T>::register_observer(
    std::function<void(T &)> callback) {
  callback(value);
  observers.emplace_back(move(callback));
}

template <class T> void fc::Util::ObservableNumber<T>::notify_observers() {
  for (auto &f : observers)
    f(value);
}

template <class T>
fc::Util::ObservableNumber<T> &
fc::Util::ObservableNumber<T>::operator+=(const T &other) {
  const lock_guard<mutex> lock(update_mutex);
  value += other;
  notify_observers();
  return *this;
}

template <class T>
fc::Util::ObservableNumber<T> &
fc::Util::ObservableNumber<T>::operator=(T other) {
  const lock_guard<mutex> lock(update_mutex);
  value = move(other);
  notify_observers();
  return *this;
}

#endif // FANCON_UTIL_HPP
