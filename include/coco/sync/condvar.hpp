#pragma once
#include "coco/sync/mutex.hpp"
#include <condition_variable>

namespace coco::sync {
class CondVar;

namespace detail {
struct [[nodiscard]] CondVarWaitAwaiter {
  constexpr auto await_ready() const noexcept -> bool { return false; }
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool;
  constexpr auto await_resume() const noexcept -> void {}

  CondVar& mCv;
};
}; // namespace detail

class CondVar {
public:
  auto wait() { return detail::CondVarWaitAwaiter{*this}; }
  auto notifyOne() -> void
  {
    WorkerJob* job = nullptr;
    mQueueMt.lock();
    if (!mWaitQueue.empty()) {
      job = mWaitQueue.popFront();
    }
    mQueueMt.unlock();
    if (job) {
      Proactor::get().execute(job, ExeOpt::PreferInOne);
    }
  }
  auto notifyAll() -> void
  {
    mQueueMt.lock();
    while (!mWaitQueue.empty()) {
      auto job = mWaitQueue.popFront();
      mQueueMt.unlock();
      Proactor::get().execute(job, ExeOpt::Balance);
      mQueueMt.lock();
    }
  }

private:
  friend struct detail::CondVarWaitAwaiter;

  coco::WorkerJobQueue mWaitQueue;
  std::mutex mQueueMt;
};

namespace detail {
template <typename Promise>
auto CondVarWaitAwaiter::await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool
{
  mCv.mQueueMt.lock();
  mCv.mWaitQueue.pushBack(awaiting.promise().getThisJob());
  mCv.mQueueMt.unlock();
  return true;
}
} // namespace detail
} // namespace coco::sync