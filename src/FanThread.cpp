#include "FanThread.hpp"

fc::FanThread::FanThread(function<void(bool &)> f)
    : t(thread([this](auto f) { f(run); }, move(f))) {}

fc::FanThread::FanThread(function<void()> f,
                         shared_ptr<Observable<int>> testing_status)
    : t(thread(move(f))), test_status(move(testing_status)) {}

fc::FanThread::~FanThread() {
  // End gracefully
  run = false;
  join();
}

bool fc::FanThread::is_testing() const { return bool(test_status); }

void fc::FanThread::join() {
  if (t.joinable()) {
    t.interrupt();
    t.join();
  }
}

fc::FanThread &fc::FanThread::operator=(fc::FanThread &&other) noexcept {
  t = move(other.t);
  test_status = other.test_status;
  return *this;
}
