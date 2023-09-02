#pragma once
#include "coco/__preclude.hpp"

#include "coco/defer.hpp"
#include "coco/timer.hpp"
#include "coco/uring.hpp"
#include "coco/worker_job.hpp"

#include <set>
#include <thread>

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
  auto execute(WorkerJobQueue&& queue) noexcept -> void { mExecutor->enqueue(std::move(queue), 0); }
  auto execute(WorkerJobQueue&& queue, std::size_t count) noexcept -> void
  {
    mExecutor->enqueue(std::move(queue), count);
  }
  auto execute(WorkerJob* job) noexcept -> void { mExecutor->enqueue(job); }
  auto addTimer(Instant time, WorkerJob* job) noexcept -> void { mTimerManager.addTimer(time, job); }
  auto deleteTimer(std::size_t jobId) noexcept -> void { mTimerManager.deleteTimer(jobId); }

  auto notify() -> void { mUring.notify(); }
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
  auto prepCancel(int fd) -> void { mUring.prepCancel(fd); }
  auto prepCancel(Token token) -> void { mUring.prepCancel(token); }
  auto prepClose(Token token, int fd) -> void { mUring.prepClose(token, fd); }

  auto wait(std::atomic_bool& notifying) -> WorkerJob*
  {
    auto [jobs, count] = mTimerManager.processTimers();
    while (auto job = jobs.popFront()) {
      job->run(job, nullptr);
    }
    auto future = mTimerManager.nextInstant();
    auto duration = future - std::chrono::steady_clock::now();
    io_uring_cqe* cqe = nullptr;
    auto e = mUring.submitWait(cqe, duration);
    if (e == std::errc::stream_timeout) {
      // timeout
    } else if (e != std::errc(0)) {
      // assert(false); // error occured
    } else {
      assert(cqe != nullptr);
      if (cqe->flags & IORING_CQE_F_MORE) {
        notifying.store(false);
      } else {
        auto job = (WorkerJob*)cqe->user_data;
        job->run(job, &cqe->res);
      }
    }
    mUring.seen(cqe);
    return nullptr;
  }

private:
  Executor* mExecutor;
  TimerManager mTimerManager{64};
  IoUring mUring{};
};
} // namespace coco