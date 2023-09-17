#pragma once

#include "coco/task.hpp"
#include "coco/util/panic.hpp"

namespace coco::sync {
class RwLock;
namespace detail {
struct RwLockReadAwaiter {
  RwLockReadAwaiter(RwLock& lock) : mLock(lock) {}

  auto await_ready() const noexcept -> bool;
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool;
  auto await_resume() const noexcept -> void {}

  RwLock& mLock;
};

struct RwLockWriteAwaiter {
  RwLockWriteAwaiter(RwLock& lock) : mLock(lock) {}

  auto await_ready() const noexcept -> bool;
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool;
  auto await_resume() const noexcept -> void {}

  RwLock& mLock;
};
} // namespace detail

class RwLock {
public:
  RwLock() = default;
  ~RwLock() = default;

  auto lockRead() { return detail::RwLockReadAwaiter(*this); }
  auto unlockRead()
  {
    auto writers = popWriter();
    if (writers != nullptr) {
      mState.store(LockState::Write, std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_acq_rel);
      Proactor::get().execute(writers, ExeOpt::prefInOne());
    } else {
      auto readers = popReaders();
      if (!readers.empty()) {
        Proactor::get().execute(std::move(readers), ExeOpt::prefInOne());
      } else {
        mState.store(LockState::Free, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acq_rel);
      }
    }
  }

  auto lockWrite() { return detail::RwLockWriteAwaiter(*this); }
  auto unlockWrite()
  {
    auto writer = popWriter();
    if (writer != nullptr) {
      Proactor::get().execute(writer, ExeOpt::prefInOne());
    } else {
      auto readers = popReaders();
      if (!readers.empty()) {
        mState.store(LockState::Read, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acq_rel);
        Proactor::get().execute(std::move(readers), ExeOpt::prefInOne());
      } else {
        mState.store(LockState::Free, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acq_rel);
      }
    }
  }

private:
  friend struct detail::RwLockReadAwaiter;
  friend struct detail::RwLockWriteAwaiter;

  auto unlock() -> void {}

  auto popWriter() -> WorkerJob*
  {
    std::scoped_lock lk(mMt);
    return mWriters.popFront();
  }

  auto popReaders() -> WorkerJobQueue
  {
    std::scoped_lock lk(mMt);
    return std::exchange(mReaders, {});
  }

  enum class LockState {
    Free,
    Read,
    Write,
  };

  std::mutex mMt;
  WorkerJobQueue mWriters;
  WorkerJobQueue mReaders;
  std::atomic<LockState> mState = LockState::Free;
};

namespace detail {
inline auto RwLockReadAwaiter::await_ready() const noexcept -> bool
{
  auto expected = RwLock::LockState::Free;
  if (mLock.mState.compare_exchange_strong(expected, RwLock::LockState::Read)) {
    return true;
  } else if (expected == RwLock::LockState::Read) {
    return true;
  } else {
    return false;
  }
}

template <typename Promise>
inline auto RwLockReadAwaiter::await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool
{
  std::scoped_lock lk(mLock.mMt);
  mLock.mReaders.pushBack(awaiting.promise().getThisJob());
  return true;
}

inline auto RwLockWriteAwaiter::await_ready() const noexcept -> bool
{
  auto expected = RwLock::LockState::Free;
  if (mLock.mState.compare_exchange_strong(expected, RwLock::LockState::Write)) {
    return true;
  } else {
    return false;
  }
}

template <typename Promise>
inline auto RwLockWriteAwaiter::await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool
{
  std::scoped_lock lk(mLock.mMt);
  mLock.mWriters.pushBack(awaiting.promise().getThisJob());
  return true;
}

} // namespace detail
} // namespace coco::sync