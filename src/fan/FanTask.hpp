#ifndef FANCON_SRC_FANTHREAD_HPP
#define FANCON_SRC_FANTHREAD_HPP

#include "util/Util.hpp"
#include "boost/thread.hpp"

using boost::thread;
using fc::Util::ObservableNumber;

namespace fc {
class FanTask {
public:
  explicit FanTask(function<void(bool &)> f);
  explicit FanTask(function<void()> f,
                   shared_ptr<ObservableNumber<int>> testing_status);
  ~FanTask();

  shared_ptr<ObservableNumber<int>> test_status = nullptr;

  bool is_testing() const;
  void join();

  FanTask &operator=(FanTask &&other) noexcept;

private:
  bool run = true;
  thread t;
};
} // namespace fc

#endif // FANCON_SRC_FANTHREAD_HPP
