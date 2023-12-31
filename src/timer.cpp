#include "coco/timer.hpp"
#include <mutex>

namespace coco {
auto TimerManager::addTimer(Instant time, WorkerJob* job) noexcept -> void
{
  std::scoped_lock lock(mPendingJobsMt);
  mPendingJobs.push({time, job, TimerOpKind::Add});
}
auto TimerManager::deleteTimer(void* jobId) noexcept -> void
{
  std::scoped_lock lock(mPendingJobsMt);
  mPendingJobs.push(TimerOp{.instant = Instant(), .jobId = jobId, .kind = TimerOpKind::Delete});
}
auto TimerManager::nextInstant() const noexcept -> Instant
{
  if (mTimers.empty()) {
    return Instant::max();
  }
  return mTimers.top().instant;
}
auto TimerManager::processTimers() -> std::pair<WorkerJobQueue, std::size_t>
{
  while (true) {
    TimerOp op{};
    {
      std::scoped_lock lock(mPendingJobsMt);
      if (!mPendingJobs.empty()) {
        op = mPendingJobs.front();
        mPendingJobs.pop();
      } else {
        break;
      }
    }
    switch (op.kind) {
    case TimerOpKind::Add: {
      mTimers.insert({op.instant, op.job});
    } break;
    case TimerOpKind::Delete: {
      mDeleted.insert(op.jobId);
    } break;
    }
  }

  WorkerJobQueue jobs;
  std::size_t count = 0;
  auto now = std::chrono::steady_clock::now();
  while (!mTimers.empty() && mTimers.top().instant <= now) {
    auto job = mTimers.top().job;
    mTimers.pop();
    if (auto it = mDeleted.find(job->state); it != mDeleted.end()) {
      mDeleted.erase(it);
      continue;
    }
    jobs.pushBack(job);
  }
  return {std::move(jobs), count};
}
} // namespace coco