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

struct WorkerJob {
  using fn_type = void (*)(WorkerJob* task, void* args) noexcept;
  WorkerJob(fn_type fn) noexcept : run(fn), next(nullptr), id(genJobId()) {}

  WorkerJob* next;
  fn_type run;
  std::size_t id;
};

using WorkerJobQueue = util::Queue<&WorkerJob::next>;

inline auto emptyFn(WorkerJob*, void*) noexcept -> void { assert(false && "empty job should not be executed"); }
inline WorkerJob emptyJob{emptyFn};

template <typename T = void>
struct Task;

class Executor {
public:
  Executor() = default;
  virtual ~Executor() noexcept = default;

  virtual auto enqueue(WorkerJobQueue&& queue, std::size_t count) noexcept -> void = 0;
  virtual auto enqueue(WorkerJob* handle) noexcept -> void = 0;
  virtual auto runMain(Task<> task) -> void = 0;
};
} // namespace coco