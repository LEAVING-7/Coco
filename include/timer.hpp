#pragma once
#include "preclude.hpp"

#include "mt_executor.hpp"
#include "util/heap.hpp"

#include <chrono>
#include <queue>
#include <unordered_set>

namespace coco {

using Instant = std::chrono::steady_clock::time_point;
using Duration = std::chrono::steady_clock::duration;

enum class TimerOpKind : std::uint8_t {
  Add,
  Delete,
};
struct TimerOp {
  Instant instant;
  union {
    WorkerJob* job;    // add a timer
    std::size_t jobId; // for delete a timer
  };
  TimerOpKind kind;
};

inline auto operator<(TimerOp const& lhs, TimerOp const& rhs) noexcept -> bool { return lhs.instant < rhs.instant; }

class TimerManager {
public:
  TimerManager() noexcept = default;
  TimerManager(std::size_t capcacity) : mTimers(capcacity), mPendingJobs() {}
  ~TimerManager() = default;

  auto addTimer(Instant time, WorkerJob* job) noexcept -> void { mPendingJobs.push({time, job, TimerOpKind::Add}); }
  auto deleteTimer(std::size_t jobId) noexcept -> void
  {
    mPendingJobs.push(TimerOp{.instant = Instant(), .jobId = jobId, .kind = TimerOpKind::Delete});
  }

  auto nextInstant() const noexcept -> Instant
  {
    if (mTimers.empty()) {
      return Instant::max();
    }
    return mTimers.top().instant;
  }

  auto processTimers() -> Queue<&WorkerJob::next>
  {
    Queue<&WorkerJob::next> jobs;
    while (!mPendingJobs.empty()) {
      auto job = mPendingJobs.front();
      mPendingJobs.pop();
      switch (job.kind) {
      case TimerOpKind::Add: {
        mTimers.insert(std::move(job));
      } break;
      case TimerOpKind::Delete: {
        mDeleted.insert(job.jobId);
      } break;
      }
    }

    auto now = std::chrono::steady_clock::now();
    while (!mTimers.empty() && mTimers.top().instant <= now) {
      auto job = mTimers.top().job;
      mTimers.pop();
      if (auto it = mDeleted.find(job->id); it != mDeleted.end()) {
        mDeleted.erase(it);
        continue;
      }
      jobs.pushBack(job);
    }
    return jobs;
  }

private:
  std::unordered_set<std::size_t> mDeleted;
  std::queue<TimerOp> mPendingJobs;
  Heap<TimerOp, 4> mTimers;
};
} // namespace coco