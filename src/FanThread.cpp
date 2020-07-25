#include "FanThread.hpp"

fc::FanThread::FanThread(thread &&t, shared_ptr<Observable<int>> testing_status)
    : t(move(t)), test_status(move(testing_status)) {}

fc::FanThread::~FanThread() { t.interrupt(); }

bool fc::FanThread::is_testing() const { return bool(test_status); }

void fc::FanThread::join() { t.join(); }

fc::FanThread &fc::FanThread::operator=(fc::FanThread &&other) noexcept {
  t = move(other.t);
  test_status = other.test_status;
  return *this;
}
