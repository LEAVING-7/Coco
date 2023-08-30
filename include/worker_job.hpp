#pragma once
#include "preclude.hpp"

#include "util/queue.hpp"

#include <coroutine>

namespace coco {
inline auto genJobId() -> std::size_t
{
  static std::atomic<std::size_t> id = 0;
  return id++;
}

struct WorkerJob {
  using fn_type = void (*)(WorkerJob* task) noexcept;
  WorkerJob(fn_type fn, bool onHeap) noexcept : run(fn), next(nullptr), id(genJobId()), onHeap(onHeap) {}

  template <typename T>
    requires std::is_base_of_v<WorkerJob, T>
  static auto freeJob(WorkerJob* job) noexcept -> void
  {
    if (job->onHeap) {
      delete reinterpret_cast<T*>(job);
    }
  }

  WorkerJob* next;
  fn_type run;
  std::size_t id : 63; // the first bit indicates the job allocation position, 0 for stack, 1 for heap
  const std::size_t onHeap : 1;
};

using WokerJobQueue = Queue<&WorkerJob::next>;

// TODO better implementation
struct CoroJob : WorkerJob {
  CoroJob(std::coroutine_handle<> handle, bool onHeap) noexcept : handle(handle), WorkerJob(&CoroJob::run, onHeap) {}
  static auto run(WorkerJob* job) noexcept -> void
  {
    auto coroJob = static_cast<CoroJob*>(job);
    coroJob->handle.resume();
    freeJob<CoroJob>(coroJob);
  }
  std::coroutine_handle<> handle;
};

class Executor {
public:
  Executor() = default;
  virtual ~Executor() noexcept = default;

  virtual auto enqueue(WokerJobQueue&& queue, std::size_t count) noexcept -> void = 0;
  virtual auto enqueue(std::coroutine_handle<> handle) noexcept -> void = 0;
};
} // namespace coco