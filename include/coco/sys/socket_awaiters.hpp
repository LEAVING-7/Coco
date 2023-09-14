#pragma once

#include "coco/proactor.hpp"
#include "coco/sys/socket_addr.hpp"
#include "coco/task.hpp"

#include <coroutine>

namespace coco::sys::detail {
template <typename Rep, typename Ratio>
static auto convertTime(std::chrono::duration<Rep, Ratio> duration, ::timeval& out) noexcept -> void
{
  auto sec = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto nsec = std::chrono::duration_cast<std::chrono::microseconds>(duration - sec);
  out.tv_sec = sec.count();
  out.tv_usec = nsec.count();
}

struct [[nodiscard]] IoJob : WorkerJob {
  IoJob(PromiseBase* pending) : WorkerJob(&IoJob::run, nullptr), mPending(pending){};
  static auto run(WorkerJob* job, void* args) noexcept -> void
  {
    auto ioJob = static_cast<IoJob*>(job);
    auto res = static_cast<int*>(args);
    ioJob->mResult = *res;
    Proactor::get().execute(ioJob->mPending->getThisJob());
  }
  int mResult;
  PromiseBase* mPending;
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
    auto job = &handle.promise();
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
    auto job = &handle.promise();
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

struct [[nodiscard]] SendMsgAwaiter : SocketAwaiter {
  template <typename... MsgArgs>
  SendMsgAwaiter(int fd, MsgArgs... args) noexcept : SocketAwaiter(fd), mIoJob(nullptr), mMsg({args...})
  {
  }
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = &handle.promise();
    mIoJob.mPending = job;

    Proactor::get().prepSendMsg(&mIoJob, mFd, &mMsg);
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
  ::msghdr mMsg;
};

struct [[nodiscard]] RecvMsgAwaiter : SocketAwaiter {
  template <typename... MsgArgs>
  RecvMsgAwaiter(int fd, MsgArgs... args) noexcept : SocketAwaiter(fd), mIoJob(nullptr), mMsg({args...})
  {
  }
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = &handle.promise();
    mIoJob.mPending = job;

    Proactor::get().prepRecvMsg(&mIoJob, mFd, &mMsg);
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
  ::msghdr mMsg;
};

// TODO: using sendto/recvfrom
inline auto SendToAwaiter(int fd, std::span<std::byte const> buf, SocketAddr addr) noexcept
    -> Task<std::pair<std::size_t, std::errc>>
{
  auto v4 = addr.toV4();
  ::iovec iov = {(void*)buf.data(), buf.size()};
  co_return co_await SendMsgAwaiter(fd, (void*)&v4, (socklen_t)sizeof(v4), &iov, 1ul, nullptr, 0ul, 0);
}
inline auto RecvFromAwaiter(int fd, std::span<std::byte> buf, SocketAddr addr) noexcept
    -> Task<std::pair<std::size_t, std::errc>>
{
  auto v4 = addr.toV4();
  ::iovec iov = {(void*)buf.data(), buf.size()};
  co_return co_await RecvMsgAwaiter(fd, &v4, (socklen_t)sizeof(v4), &iov, 1ul, nullptr, 0ul, 0);
}

struct [[nodiscard]] ConnectAwaiter : SocketAwaiter {
  ConnectAwaiter(int fd, SocketAddr addr) noexcept : SocketAwaiter(fd), mIoJob(nullptr), mAddr(addr) {}

  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = &handle.promise();
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
    auto job = &handle.promise();
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

struct [[nodiscard]] RecvTimeoutAwaiter : RecvAwaiter {
  RecvTimeoutAwaiter(int fd, std::span<std::byte> buf, Duration timeout) noexcept
      : RecvAwaiter(fd, buf), mTimeout(timeout)
  {
  }
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = &handle.promise();
    mIoJob.mPending = job;

    mProactor = &Proactor::get();
    mProactor->prepRecv(&mIoJob, mFd, mBuf);
    mProactor->prepTimeout(&mIoJob, mTimeout);
  }
  auto await_resume() noexcept -> std::pair<std::size_t, std::errc>
  {
    if (mIoJob.mResult < 0) {
      if (mIoJob.mResult == -ETIME) {
        mProactor->delPendingSet(CancelItem::cancelIo(mFd));
        return {0, std::errc::timed_out};
      } else {
        return {0, std::errc(-mIoJob.mResult)};
      }
    } else {
      mProactor->delPendingSet(CancelItem::cancelTimeout(&mIoJob));
      return {std::size_t(mIoJob.mResult), std::errc(0)};
    }
  }

  Proactor* mProactor;
  Duration mTimeout;
};

struct [[nodiscard]] SendTimeoutAwaiter : SendAwaiter {
  SendTimeoutAwaiter(int fd, std::span<std::byte const> buf, Duration timeout) noexcept
      : SendAwaiter(fd, buf), mTimeout(timeout)
  {
  }
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = &handle.promise();
    mIoJob.mPending = job;

    mProactor = &Proactor::get();
    mProactor->prepSend(&mIoJob, mFd, mBuf);
    mProactor->prepTimeout(&mIoJob, mTimeout);
  }
  auto await_resume() noexcept -> std::pair<std::size_t, std::errc>
  {
    if (mIoJob.mResult < 0) {
      if (mIoJob.mResult == -ETIME) {
        mProactor->delPendingSet(CancelItem::cancelIo(mFd));
        return {0, std::errc::timed_out};
      } else {
        return {0, std::errc(-mIoJob.mResult)};
      }
    } else {
      mProactor->delPendingSet(CancelItem::cancelTimeout(&mIoJob));
      return {std::size_t(mIoJob.mResult), std::errc(0)};
    }
  }

  Proactor* mProactor;
  Duration mTimeout;
};

struct [[nodiscard]] AcceptTimeoutAwaiter : AcceptAwaiter {
  AcceptTimeoutAwaiter(int fd, Duration timeout) noexcept : AcceptAwaiter(fd), mTimeout(timeout) {}

  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = &handle.promise();
    mIoJob.mPending = job;

    mProactor = &Proactor::get();
    mProactor->prepAccept(&mIoJob, mFd, nullptr, nullptr);
    mProactor->prepTimeout(&mIoJob, mTimeout);
  }

  auto await_resume() noexcept -> std::pair<int, std::errc>
  {
    if (mIoJob.mResult < 0) {
      if (mIoJob.mResult == -ETIME) {
        mProactor->delPendingSet(CancelItem::cancelIo(mFd));
        return {0, std::errc::timed_out};
      } else {
        return {0, std::errc(-mIoJob.mResult)};
      }
    } else {
      mProactor->delPendingSet(CancelItem::cancelTimeout(&mIoJob));
      return {mIoJob.mResult, std::errc(0)};
    }
  }

  Proactor* mProactor;
  Duration mTimeout;
};

struct [[nodiscard]] ConnectTimeoutAwaiter : ConnectAwaiter {
  ConnectTimeoutAwaiter(int fd, SocketAddr addr, Duration timeout) noexcept
      : ConnectAwaiter(fd, addr), mTimeout(timeout)
  {
  }

  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
  {
    auto job = &handle.promise();
    mIoJob.mPending = job;

    mProactor = &Proactor::get();
    auto v4 = mAddr.toV4();
    mProactor->prepConnect(&mIoJob, mFd, (sockaddr*)&v4, sizeof(v4));
    mProactor->prepTimeout(&mIoJob, mTimeout);
  }

  auto await_resume() noexcept -> std::errc
  {
    if (mIoJob.mResult < 0) {
      if (mIoJob.mResult == -ETIME) {
        mProactor->delPendingSet(CancelItem::cancelIo(mFd));
        return std::errc::timed_out;
      } else {
        return std::errc(-mIoJob.mResult);
      }
    } else {
      mProactor->delPendingSet(CancelItem::cancelTimeout(&mIoJob));
      return std::errc(0);
    }
  }

  Proactor* mProactor;
  Duration mTimeout;
};
}; // namespace coco::sys::detail