#pragma once
#include "coco/task.hpp"
#include "coco/util/panic.hpp"

namespace coco::sync {
class Latch;
namespace detail {
struct [[nodiscard]] LatchWaitAwaiter {
  LatchWaitAwaiter(Latch& latch) : mLatch(latch) {}

  auto await_ready() const noexcept -> bool;
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool;
  constexpr auto await_resume() const noexcept -> void {}

  Latch& mLatch;
};

struct [[nodiscard]] LatchArriveAndWaitAwaiter {
  LatchArriveAndWaitAwaiter(Latch& latch) : mLatch(latch) {}

  auto await_ready() const noexcept -> bool;
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool;
  constexpr auto await_resume() const noexcept -> void {}

  Latch& mLatch;
};
} // namespace detail

class Latch {
public:
  Latch(std::size_t count) : mCount(count) {}
  auto countDown() -> void
  {
    if (mCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      wakeAll();
    }
  }
  auto wait() { return detail::LatchWaitAwaiter(*this); }
  auto arriveAndWait() { return detail::LatchArriveAndWaitAwaiter(*this); }

private:
  auto wakeAll() -> void
  {
    auto queue = mWaiting.popAll();
    while (auto task = queue.popFront()) {
      Proactor::get().execute(task, ExeOpt::balance());
    }
  }

  friend struct detail::LatchWaitAwaiter;
  friend struct detail::LatchArriveAndWaitAwaiter;

  util::AtomicQueue<&WorkerJob::next> mWaiting;
  std::atomic_size_t mCount;
};

namespace detail {
inline auto LatchWaitAwaiter::await_ready() const noexcept -> bool
{
  if (mLatch.mCount.load(std::memory_order_acquire) == 0) {
    mLatch.wakeAll();
    return true;
  } else {
    return false;
  };
}

template <typename Promise>
auto LatchWaitAwaiter::await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool
{
  mLatch.mWaiting.pushFront(awaiting.promise().getThisJob());
  return true;
}

inline auto LatchArriveAndWaitAwaiter::await_ready() const noexcept -> bool
{
  if (mLatch.mCount.fetch_sub(1, std::memory_order_acquire) == 1) {
    mLatch.wakeAll();
    return true;
  } else {
    return false;
  }
}

template <typename Promise>
inline auto LatchArriveAndWaitAwaiter::await_suspend(std::coroutine_handle<Promise> awaiting) noexcept -> bool
{
  mLatch.mWaiting.pushFront(awaiting.promise().getThisJob());
  return true;
}

} // namespace detail
} // namespace coco::sync