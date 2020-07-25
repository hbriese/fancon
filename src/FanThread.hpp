#ifndef FANCON_SRC_FANTHREAD_HPP
#define FANCON_SRC_FANTHREAD_HPP

#include "Util.hpp"
#include "boost/thread.hpp"

using boost::thread;
using fc::Util::Observable;

namespace fc {
class FanThread {
public:
  explicit FanThread(thread &&t,
                     shared_ptr<Observable<int>> testing_status = nullptr);
  ~FanThread();

  thread t;
  shared_ptr<Observable<int>> test_status = nullptr;

  bool is_testing() const;
  void join();
  FanThread &operator=(FanThread &&other) noexcept;
};
} // namespace fc

#endif // FANCON_SRC_FANTHREAD_HPP
