#pragma once
#include "coco/__preclude.hpp"

#include "coco/proactor.hpp"
#include "coco/sys/socket_awaiters.hpp" // for IoJob
#include <coroutine>

namespace coco::sys::detail {
struct [[nodiscard]] FileAwaiter {
  FileAwaiter(int fd) noexcept : mFd(fd) {}
  auto await_ready() const noexcept -> bool { return false; }
  int mFd;
};

struct [[nodiscard]] ReadAwaiter : FileAwaiter {
  ReadAwaiter(int fd, std::span<std::byte> buf, off_t offset) noexcept
      : FileAwaiter(fd), mIoJob(nullptr), mBuf(buf), mOffset(offset)
  {
  }

  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = handle.promise().getThisJob();
    mIoJob.mPending = job;
    Proactor::get().prepRead(&mIoJob, mFd, mBuf, mOffset);
  }
  auto await_resume() noexcept -> std::pair<std::size_t, std::errc>
  {
    if (mIoJob.mResult < 0) {
      return {0, std::errc(-mIoJob.mResult)};
    } else {
      return {std::size_t(mIoJob.mResult), std::errc(0)};
    }
  }

  IoJob mIoJob;
  off_t mOffset;
  std::span<std::byte> mBuf;
};

struct [[nodiscard]] WriteAwaiter : FileAwaiter {
  WriteAwaiter(int fd, std::span<std::byte const> buf, off_t offset) noexcept
      : FileAwaiter(fd), mIoJob(nullptr), mBuf(buf), mOffset(offset)
  {
  }

  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = handle.promise().getThisJob();
    mIoJob.mPending = job;
    Proactor::get().prepWrite(&mIoJob, mFd, mBuf, mOffset);
  }
  auto await_resume() noexcept -> std::pair<std::size_t, std::errc>
  {
    if (mIoJob.mResult < 0) {
      return {0, std::errc(-mIoJob.mResult)};
    } else {
      return {std::size_t(mIoJob.mResult), std::errc(0)};
    }
  }

  IoJob mIoJob;
  off_t mOffset;
  std::span<std::byte const> mBuf;
};
} // namespace coco::sys::detail