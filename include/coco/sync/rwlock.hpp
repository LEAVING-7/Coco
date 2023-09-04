#pragma once

#include "coco/task.hpp"

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

  auto read() { return detail::RwLockReadAwaiter(*this); }
  auto write() { return detail::RwLockWriteAwaiter(*this); }

private:
  friend struct detail::RwLockReadAwaiter;
  friend struct detail::RwLockWriteAwaiter;

  std::mutex mMt;
};
} // namespace coco::sync