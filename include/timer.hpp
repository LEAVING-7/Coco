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

enum class TimerJobKind : std::uint8_t {
  Add,
  Delete,
};
struct TimerJob {
  Instant instant;
  union {
    WorkerJob* job;    // add a timer
    std::size_t jobId; // for delete a timer
  };
  TimerJobKind kind;
};

inline auto operator<(TimerJob const& lhs, TimerJob const& rhs) noexcept -> bool { return lhs.instant < rhs.instant; }

class TimerManager {
public:
  TimerManager() noexcept = default;
  TimerManager(std::size_t capcacity) : mTimers(capcacity), mPendingJobs() {}
  ~TimerManager() = default;

  auto addTimer(Instant time, WorkerJob* job) noexcept -> void { mPendingJobs.push({time, job, TimerJobKind::Add}); }
  auto deleteTimer(std::size_t jobId) noexcept -> void
  {
    mPendingJobs.push(TimerJob{.instant = Instant(), .jobId = jobId, .kind = TimerJobKind::Delete});
  }
  auto nextInstant() -> Duration
  {
    auto instant = processTimers();
    if (instant == Instant()) {
      return Duration::zero();
    } else {
      return instant - std::chrono::steady_clock::now();
    }
  }

  auto processTimers() -> Instant
  {
    std::unordered_set<std::size_t> deleted;
    while (!mPendingJobs.empty()) {
      auto job = mPendingJobs.front();
      mPendingJobs.pop();
      switch (job.kind) {
      case TimerJobKind::Add: {
        mTimers.insert(std::move(job));
      } break;
      case TimerJobKind::Delete: {
        deleted.insert(job.jobId);
      } break;
      }
    }
    ::printf("timer size %zu\n", mTimers.size());

    auto now = std::chrono::steady_clock::now();
    while (!mTimers.empty() && mTimers.top().instant <= now) {
      auto job = mTimers.top().job;
      mTimers.pop();
      if (deleted.contains(job->id)) {
        continue;
      }
      UringInstance::get().execute(job);
    }

    if (mTimers.empty()) {
      return Instant();
    } else {
      return mTimers.top().instant;
    }
  }

  auto _debugProcessTimers() -> std::vector<WorkerJob*>
  {
    std::vector<WorkerJob*> jobs;
    while (!mPendingJobs.empty()) {
      auto job = mPendingJobs.front();
      mPendingJobs.pop();
      switch (job.kind) {
      case TimerJobKind::Add: {
        mTimers.insert(std::move(job));
      } break;
      case TimerJobKind::Delete: {
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
      jobs.push_back(job);
    }
    return jobs;
  }

private:
  std::unordered_set<std::size_t> mDeleted;
  std::queue<TimerJob> mPendingJobs;
  Heap<TimerJob, 4> mTimers;
};
} // namespace coco