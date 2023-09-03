#pragma once
#include "coco/__preclude.hpp"

#include "coco/task.hpp"

// FIXME: not finished and not efficient
namespace coco::sync {
class Mutex;
namespace detail {
struct MutexLockAwaiter {
  MutexLockAwaiter(Mutex& mt) : mMt(mt) {}
  auto await_ready() const noexcept -> bool;
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> void;
  auto await_resume() const noexcept -> void {}

  Mutex& mMt;
};

struct MutexTryLockAwaiter {
  MutexTryLockAwaiter(Mutex& mt) : mMt(mt) {}
  auto await_ready() const noexcept -> bool;
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool;
  auto await_resume() const noexcept -> bool;

  Mutex& mMt;
  bool mSuccess = false;
};
}; // namespace detail
class Mutex {
public:
  Mutex() = default;
  ~Mutex() = default;

  auto lock() -> detail::MutexLockAwaiter { return detail::MutexLockAwaiter(*this); }
  auto tryLock() -> detail::MutexTryLockAwaiter { return detail::MutexTryLockAwaiter(*this); }
  auto unlock() -> void
  {
    mQueueMt.lock();
    if (mWaitQueue.empty()) {
      mHold.store(false, std::memory_order_relaxed);
      mQueueMt.unlock();
      return;
    }
    auto job = mWaitQueue.popFront();
    mQueueMt.unlock();
    Proactor::get().execute(job, ExeOpt::OneThread);
  }

private:
  friend struct detail::MutexLockAwaiter;
  friend struct detail::MutexTryLockAwaiter;

  coco::WorkerJobQueue mWaitQueue;
  std::mutex mQueueMt; // Better solution ?
  std::atomic_bool mHold = false;
};

namespace detail {
inline auto MutexLockAwaiter::await_ready() const noexcept -> bool
{
  return !mMt.mHold.exchange(true, std::memory_order_relaxed);
}
template <typename Promise>
auto MutexLockAwaiter::await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> void
{
  auto job = awaiting.promise().getThisJob();
  std::scoped_lock lk(mMt.mQueueMt);
  mMt.mWaitQueue.pushBack(job);
}

inline auto MutexTryLockAwaiter::await_ready() const noexcept -> bool { return false; }
template <typename Promise>
auto MutexTryLockAwaiter::await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool
{
  if (mMt.mHold.exchange(true, std::memory_order_acquire)) {
    mSuccess = false;
  } else {
    mSuccess = true;
  }
  return false;
}
inline auto MutexTryLockAwaiter::await_resume() const noexcept -> bool { return mSuccess; }
}; // namespace detail
} // namespace coco::sync