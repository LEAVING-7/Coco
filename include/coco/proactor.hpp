#pragma once
#include "coco/__preclude.hpp"

#include "coco/defer.hpp"
#include "coco/timer.hpp"
#include "coco/uring.hpp"
#include "coco/worker_job.hpp"

namespace coco {
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
  auto deleteTimer(std::size_t jobId) noexcept -> void { mTimerManager.deleteTimer(jobId); }
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
    mUring.prepRecv(token, fd, buf, flag);
  }
  auto prepSend(Token token, int fd, std::span<std::byte const> buf, int flag = 0) -> void
  {
    mUring.prepSend(token, fd, buf, flag);
  }
  auto prepAccept(Token token, int fd, sockaddr* addr, socklen_t* addrlen, int flags = 0) -> void
  {
    mUring.prepAccept(token, fd, addr, addrlen, flags);
  }
  auto prepConnect(Token token, int fd, sockaddr* addr, socklen_t addrlen) -> void
  {
    mUring.prepConnect(token, fd, addr, addrlen);
  }
  auto prepRead(Token token, int fd, std::span<std::byte> buf, off_t offset) -> void
  {
    mUring.prepRead(token, fd, buf, offset);
  }
  auto prepWrite(Token token, int fd, std::span<std::byte const> buf, off_t offset) -> void
  {
    mUring.prepWrite(token, fd, buf, offset);
  }
  auto prepCancel(int fd) -> void { mUring.prepCancel(fd); }
  auto prepCancel(Token token) -> void { mUring.prepCancel(token); }
  auto prepClose(Token token, int fd) -> void { mUring.prepClose(token, fd); }

  auto wait() -> void
  {
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
        auto job = (WorkerJob*)cqe->user_data;
        runJob(job, &cqe->res);
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
    io_uring_cqe* cqe = nullptr;
    io_uring_for_each_cqe(mUring.uring(), head, cqe)
    {
      if (cqe->flags & IORING_CQE_F_MORE) {
        mNotifyBlocked.store(false);
      } else {
        auto job = (WorkerJob*)cqe->user_data;
        runJob(job, &cqe->res);
      }
      count++;
    }
    mUring.advance(count);
  }

private:
  Executor* mExecutor;
  TimerManager mTimerManager{64};
  IoUring mUring{};
  std::atomic_bool mNotifyBlocked{false};
};
} // namespace coco