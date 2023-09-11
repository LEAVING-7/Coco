#pragma once

#include "coco/task.hpp"

// FIXME: not efficient
namespace coco::sync {
class Mutex;
namespace detail {
struct [[nodiscard]] MutexLockAwaiter {
  MutexLockAwaiter(Mutex& mt) : mMt(mt) {}
  auto await_ready() const noexcept -> bool;
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool;
  auto await_resume() const noexcept -> void {}

  Mutex& mMt;
};

struct [[nodiscard]] MutexTryLockAwaiter {
  MutexTryLockAwaiter(Mutex& mt) : mMt(mt) {}
  auto await_ready() const noexcept -> bool;
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool;
  auto await_resume() const noexcept -> bool;

  Mutex& mMt;
  bool mSuccess = false;
};
}; // namespace detail
template <typename MutexTy>
class LockGuard {
public:
  LockGuard() = default; // TODO delete
  LockGuard(MutexTy* mt) : mMt(mt) {}
  ~LockGuard() { mMt->unlock(); }

private:
  MutexTy* mMt;
};

class Mutex {
public:
  Mutex() = default;
  ~Mutex() = default;

  auto lock() -> detail::MutexLockAwaiter { return detail::MutexLockAwaiter(*this); }
  auto tryLock() -> detail::MutexTryLockAwaiter { return detail::MutexTryLockAwaiter(*this); }
  auto guard() -> coco::Task<LockGuard<Mutex>>
  {
    co_await lock();
    co_return LockGuard<Mutex>(this);
  }
  auto unlock() -> void
  {
    mQueueMt.lock();
    if (mWaitQueue.empty()) {
      mHold.store(false, std::memory_order_release);
      mQueueMt.unlock();
      return;
    }
    auto job = mWaitQueue.popFront();
    mHold.store(false, std::memory_order_release);
    mQueueMt.unlock();
    Proactor::get().execute(job, ExeOpt::PreferInOne);
  }

private:
  friend struct detail::MutexLockAwaiter;
  friend struct detail::MutexTryLockAwaiter;
  friend struct CondVar;

  coco::WorkerJobQueue mWaitQueue;
  std::mutex mQueueMt;
  std::atomic_bool mHold = false;
};

namespace detail {
inline auto MutexLockAwaiter::await_ready() const noexcept -> bool { return false; }
template <typename Promise>
auto MutexLockAwaiter::await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool
{
  auto expected = false;
  if (!mMt.mHold.compare_exchange_strong(expected, true)) {
    auto job = awaiting.promise().getThisJob();
    std::scoped_lock lk(mMt.mQueueMt);
    mMt.mWaitQueue.pushBack(job);
    return true;
  } else {
    return false;
  }
}

inline auto MutexTryLockAwaiter::await_ready() const noexcept -> bool { return false; }
template <typename Promise>
auto MutexTryLockAwaiter::await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool
{
  auto expected = false;
  if (mMt.mHold.compare_exchange_strong(expected, true)) {
    mSuccess = true;
    return false;
  } else {
    mSuccess = false;
    return false;
  };
}
inline auto MutexTryLockAwaiter::await_resume() const noexcept -> bool { return mSuccess; }
}; // namespace detail
} // namespace coco::sync