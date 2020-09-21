#include "FanTask.hpp"

fc::FanTask::FanTask(function<void(bool &)> f)
    : t(thread([this](auto f) { f(run); }, move(f))) {}

fc::FanTask::FanTask(function<void()> f,
                     shared_ptr<ObservableNumber<int>> testing_status)
    : test_status(move(testing_status)), t(thread(move(f))) {}

fc::FanTask::~FanTask() { join(); }

bool fc::FanTask::is_testing() const { return bool(test_status); }

void fc::FanTask::join() {
  // End gracefully
  run = false;
  if (t.joinable()) {
    t.join();
  }
}

fc::FanTask &fc::FanTask::operator=(fc::FanTask &&other) noexcept {
  t = move(other.t);
  test_status = other.test_status;
  return *this;
}
