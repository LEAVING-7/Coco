#pragma once
#include "coco/__preclude.hpp"

#include "coco/util/queue.hpp"

#include <coroutine>

namespace coco {
inline auto genJobId() -> std::size_t
{
  static std::atomic_size_t id = 0;
  return id.fetch_add(1, std::memory_order_relaxed);
}

enum class JobState : std::uint16_t {
  Ready,
  Executing,
  Pending,
  Cancel,
  Final,
};

enum class JobAction : std::uint16_t {
  None,
  Detach,
  NotifyAction,
  NotifyState,
  OneShot,
  Final,
};

struct WorkerJob {
  using fn_type = void (*)(WorkerJob* task, void* args) noexcept;
  WorkerJob(fn_type fn) noexcept : run(fn), next(nullptr), id(genJobId()) {}

  WorkerJob* next;
  fn_type run;
  std::uint32_t id;
  std::atomic<JobState> state{JobState::Ready};
  std::atomic<JobAction> action{JobAction::None};
};

using WorkerJobQueue = util::Queue<&WorkerJob::next>;

inline auto runJob(WorkerJob* job, void* args) noexcept -> void
{
  assert(job != nullptr && "job should not be nullptr");
  JobState expected = JobState::Ready;
  auto action = job->action.load(std::memory_order_relaxed);
  if (job->state.compare_exchange_strong(expected, JobState::Executing)) {
    job->run(job, args);
    if (action != JobAction::OneShot) {
      JobState expected = JobState::Executing;
      if (job->state.compare_exchange_strong(expected, JobState::Ready)) {
        // do nothing
      } else {
        // already done
      };
    }
  } else {
  }
}

inline auto emptyFn(WorkerJob*, void*) noexcept -> void { assert(false && "empty job should not be executed"); }
inline WorkerJob emptyJob{emptyFn};

template <typename T = void>
struct Task;

enum class ExeOpt {
  Balance,
  OneThread,
};

class Executor {
public:
  Executor() = default;
  virtual ~Executor() noexcept = default;

  virtual auto execute(WorkerJobQueue&& queue, std::size_t count, ExeOpt opt) noexcept -> void = 0;
  virtual auto execute(WorkerJob* handle, ExeOpt opt) noexcept -> void = 0;
  virtual auto runMain(Task<> task) -> void = 0;
};
} // namespace coco