#pragma once

#include "coco/defer.hpp"
#include "coco/timer.hpp"
#include "coco/uring.hpp"
#include "coco/worker_job.hpp"

namespace coco {
struct CancelItem {
  enum class Kind { IoFd, TimeoutToken } mKind;
  union {
    int mFd;
    Token mToken;
  };
  static auto cancelIo(int fd) -> CancelItem { return {Kind::IoFd, fd}; }
  static auto cancelTimeout(Token token) -> CancelItem { return {.mKind = Kind::TimeoutToken, .mToken = token}; }
};
class Proactor {
public:
  static auto get() noexcept -> Proactor&
  {
    static thread_local auto instance = Proactor();
    return instance;
  }

  Proactor() = default;
  ~Proactor() = default;

  auto attachExecutor(Executor* executor) noexcept -> void { mExecutor = executor; }
  auto execute(WorkerJobQueue&& queue, ExeOpt opt = ExeOpt::Balance) noexcept -> void
  {
    mExecutor->execute(std::move(queue), 0, opt);
    notify();
  }
  auto execute(WorkerJobQueue&& queue, std::size_t count, ExeOpt opt = ExeOpt::Balance) noexcept -> void
  {
    mExecutor->execute(std::move(queue), count, opt);
    notify();
  }
  auto execute(WorkerJob* job, ExeOpt opt = ExeOpt::Balance) noexcept -> void
  {
    mExecutor->execute(job, opt);
    notify();
  }
  auto addTimer(Instant time, WorkerJob* job) noexcept -> void { mTimerManager.addTimer(time, job); }
  auto deleteTimer(void* jobId) noexcept -> void { mTimerManager.deleteTimer(jobId); }
  auto processTimers() { return mTimerManager.processTimers(); }

  auto notify() -> void
  {
    bool expected = false;
    if (mNotifyBlocked.compare_exchange_strong(expected, true)) {
      mUring.notify();
    }
  }
  auto prepRecv(Token token, int fd, std::span<std::byte> buf, int flag = 0) -> void
  {
    addPendingSet((WorkerJob*)token);
    mUring.prepRecv(token, fd, buf, flag);
  }
  auto prepSend(Token token, int fd, std::span<std::byte const> buf, int flag = 0) -> void
  {
    addPendingSet((WorkerJob*)token);
    mUring.prepSend(token, fd, buf, flag);
  }
  auto prepAccept(Token token, int fd, sockaddr* addr, socklen_t* addrlen, int flags = 0) -> void
  {
    addPendingSet((WorkerJob*)token);
    mUring.prepAccept(token, fd, addr, addrlen, flags);
  }
  auto prepConnect(Token token, int fd, sockaddr* addr, socklen_t addrlen) -> void
  {
    addPendingSet((WorkerJob*)token);
    mUring.prepConnect(token, fd, addr, addrlen);
  }
  template <typename Rep, typename Period>
  auto prepTimeout(Token token, std::chrono::duration<Rep, Period> duration) -> void
  {
    addPendingSet((WorkerJob*)token);
    mUring.prepAddTimeout(token, duration);
  }
  template <typename Rep, typename Period>
  auto prepUpdateTimeout(Token token, std::chrono::duration<Rep, Period> duration) -> void
  {
    addPendingSet((WorkerJob*)token);
    mUring.prepUpdateTimeout(token, duration);
  }
  auto prepRemoveTimeout(Token token) -> void { mUring.prepRemoveTimeout(token); }
  auto prepRecvMsg(Token token, int fd, msghdr* msg, unsigned flag = 0) -> void
  {
    addPendingSet((WorkerJob*)token);
    mUring.prepRecvMsg(token, fd, msg, flag);
  }
  auto prepSendMsg(Token token, int fd, msghdr* msg, unsigned flag = 0) -> void
  {
    addPendingSet((WorkerJob*)token);
    mUring.prepSendMsg(token, fd, msg, flag);
  }
  auto prepRead(Token token, int fd, std::span<std::byte> buf, off_t offset) -> void
  {
    addPendingSet((WorkerJob*)token);
    mUring.prepRead(token, fd, buf, offset);
  }
  auto prepWrite(Token token, int fd, std::span<std::byte const> buf, off_t offset) -> void
  {
    addPendingSet((WorkerJob*)token);
    mUring.prepWrite(token, fd, buf, offset);
  }
  auto prepCancel(int fd) -> void { mUring.prepCancel(fd); }
  auto prepCancel(Token token) -> void { mUring.prepCancel(token); }
  auto prepClose(Token token, int fd) -> void { mUring.prepClose(token, fd); }

  auto addCancel(CancelItem cancel) -> void
  {
    std::lock_guard<std::mutex> lock(mCancelMt);
    mCancels.push_back(cancel);
    notify();
  }

  auto wait() -> void
  {
    processCancel();
    auto [jobs, count] = mTimerManager.processTimers();
    while (auto job = jobs.popFront()) {
      runJob(job, nullptr);
    }
    auto future = mTimerManager.nextInstant();
    auto duration = future - std::chrono::steady_clock::now();
    if (mNotifyBlocked) {
      submit();
    } else {
      submitWait(duration);
    }
    return;
  }

  auto delPendingSet(CancelItem item) -> void
  {
    std::lock_guard<std::mutex> lock(mPendingSet);
    switch (item.mKind) {
    case CancelItem::Kind::IoFd:
      break;
    case CancelItem::Kind::TimeoutToken:
      mPendingJobs.erase((WorkerJob*)item.mToken);
      break;
    }
    doCancel(item);
  }

private:
  template <typename Rep, typename Period>
  auto submitWait(std::chrono::duration<Rep, Period> duration) -> void
  {
    io_uring_cqe* cqe = nullptr;
    auto e = mUring.submitWait(cqe, duration);
    if (e == std::errc::stream_timeout) {
      // timeout
    } else if (e != std::errc(0)) {
      // error occured
    } else if (cqe != nullptr) {
      if (cqe->flags & IORING_CQE_F_MORE) {
        mNotifyBlocked.store(false);
      } else {
        doIoJob(cqe);
      }
      mUring.seen(cqe);
    }
  }

  auto submit() -> void
  {
    auto e = mUring.submit();
    if (e != std::errc(0)) {
      assert(false); // error occured
    }
    std::uint32_t count = 0;
    std::uint32_t head = 0;
    ::io_uring_cqe* cqe = nullptr;
    io_uring_for_each_cqe(mUring.uring(), head, cqe)
    {
      if (cqe->flags & IORING_CQE_F_MORE) {
        mNotifyBlocked.store(false);
      } else {
        doIoJob(cqe);
      }
      count++;
    }
    mUring.advance(count);
  }

  auto processCancel() -> void
  {
    std::lock_guard<std::mutex> lock(mCancelMt);
    while (!mCancels.empty()) {
      auto cancel = mCancels.back();
      mCancels.pop_back();
      doCancel(cancel);
    }
    auto r = mUring.submit();
    assert(r == std::errc(0));
  }

  auto addPendingSet(WorkerJob* job) -> void
  {
    std::lock_guard<std::mutex> lock(mPendingSet);
    mPendingJobs.insert(job);
  }

private:
  auto doIoJob(::io_uring_cqe* cqe) noexcept -> void
  {
    auto job = (WorkerJob*)cqe->user_data;
    if (job != nullptr) {
      auto n = 0;
      {
        std::lock_guard<std::mutex> lock(mPendingSet);
        n = mPendingJobs.erase(job);
      }
      if (n == 1) {
        runJob(job, &cqe->res);
      }
    }
  }

  auto doCancel(CancelItem item) noexcept -> void
  {
    switch (item.mKind) {
    case CancelItem::Kind::IoFd:
      mUring.prepCancel(item.mFd);
      break;
    case CancelItem::Kind::TimeoutToken:
      mUring.prepRemoveTimeout(item.mToken);
      break;
    }
  }

  Executor* mExecutor;
  TimerManager mTimerManager{64};
  IoUring mUring{};
  std::mutex mPendingSet;
  std::unordered_set<WorkerJob*> mPendingJobs;

  std::mutex mCancelMt;
  std::vector<CancelItem> mCancels;
  std::atomic_bool mNotifyBlocked{false};
};
} // namespace coco