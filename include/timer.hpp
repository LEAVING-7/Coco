#pragma once
#include "preclude.hpp"

#include "woker_job.hpp"
#include "util/heap.hpp"
#include "util/lockfree_queue.hpp"

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

  // MT-Safe
  auto addTimer(Instant time, WorkerJob* job) noexcept -> void;
  // MT-Safe
  auto deleteTimer(std::size_t jobId) noexcept -> void;
  auto nextInstant() const noexcept -> Instant;
  auto processTimers() -> std::pair<WokerJobQueue, std::size_t>;

private:
  std::mutex mPendingJobsMt;
  std::queue<TimerOp> mPendingJobs;
  std::unordered_set<std::size_t> mDeleted;
  Heap<TimerOp, 4> mTimers;
};
} // namespace coco