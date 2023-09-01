#pragma once
#include "coco/__preclude.hpp"
#include "coco/proactor.hpp"
#include "coco/net/socket_addr.hpp"

#include <coroutine>

namespace coco::net::detail {
struct [[nodiscard]] IoJob : WorkerJob {
  IoJob(WorkerJob* pending) : WorkerJob(&IoJob::run), mPending(pending){};
  static auto run(WorkerJob* job, void* args) noexcept -> void
  {
    auto ioJob = static_cast<IoJob*>(job);
    auto res = static_cast<int*>(args);
    ioJob->mResult = *res;
    Proactor::get().execute(ioJob->mPending);
  }
  int mResult;
  WorkerJob* mPending;
};

struct SocketAwaiter {
  SocketAwaiter(int fd) noexcept : mFd(fd) {}
  auto await_ready() const noexcept -> bool { return false; }
  int mFd;
};

struct [[nodiscard]] RecvAwaiter : SocketAwaiter {
  RecvAwaiter(int fd, std::span<std::byte> buf) noexcept : SocketAwaiter(fd), mIoJob(nullptr), mBuf(buf) {}

  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = handle.promise().getThisJob();
    mIoJob.mPending = job;
    Proactor::get().prepRecv(&mIoJob, mFd, mBuf);
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
  std::span<std::byte> mBuf;
};

struct [[nodiscard]] SendAwaiter : SocketAwaiter {
  SendAwaiter(int fd, std::span<std::byte const> buf) noexcept : SocketAwaiter(fd), mIoJob(nullptr), mBuf(buf) {}
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = handle.promise().getThisJob();
    mIoJob.mPending = job;
    Proactor::get().prepSend(&mIoJob, mFd, mBuf);
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
  std::span<std::byte const> mBuf;
};

struct [[nodiscard]] ConnectAwaiter : SocketAwaiter {
  ConnectAwaiter(int fd, SocketAddr addr) noexcept : SocketAwaiter(fd), mIoJob(nullptr), mAddr(addr) {}

  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = handle.promise().getThisJob();
    mIoJob.mPending = job;
    auto v4 = mAddr.toV4();
    Proactor::get().prepConnect(&mIoJob, mFd, (sockaddr*)&v4, sizeof(v4));
  }
  auto await_resume() noexcept -> std::errc
  {
    if (mIoJob.mResult < 0) {
      return std::errc(-mIoJob.mResult);
    } else {
      return std::errc(0);
    }
  }
  IoJob mIoJob;
  SocketAddr mAddr;
};

struct [[nodiscard]] AcceptAwaiter : SocketAwaiter {
  AcceptAwaiter(int fd) noexcept : SocketAwaiter(fd), mIoJob(nullptr) {}

  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = handle.promise().getThisJob();
    mIoJob.mPending = job;
    Proactor::get().prepAccept(&mIoJob, mFd, nullptr, nullptr);
  }

  auto await_resume() noexcept -> std::pair<int, std::errc>
  {
    if (mIoJob.mResult < 0) {
      return {0, std::errc(-mIoJob.mResult)};
    } else {
      return {mIoJob.mResult, std::errc(0)};
    }
  }
  IoJob mIoJob;
};
}; // namespace coco::net::detail