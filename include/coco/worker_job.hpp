#pragma once

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
  Final,
};

struct WorkerJob {
  using WorkerFn = void (*)(WorkerJob* task, void* args) noexcept;
  WorkerJob(WorkerFn fn, std::atomic<JobState>* state) noexcept : run(fn), next(nullptr), state(state) {}

  WorkerFn run;
  WorkerJob* next;
  std::atomic<JobState>* state;
};

using WorkerJobQueue = util::Queue<&WorkerJob::next>;

inline auto runJob(WorkerJob* job, void* args) noexcept -> void
{
  assert(job != nullptr && "job should not be nullptr");
  job->run(job, args);
}

inline auto emptyFn(WorkerJob*, void*) noexcept -> void { assert(false && "empty job should not be executed"); }
inline WorkerJob emptyJob{emptyFn, nullptr};

template <typename T = void>
struct Task;

enum class ExeOpt {
  Balance,
  PreferInOne,
  ForceInOne,
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