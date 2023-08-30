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
  WorkerJob(fn_type fn) noexcept : run(fn), next(nullptr), id(genJobId()) {}

  WorkerJob* next;
  void (*run)(WorkerJob* task) noexcept;
  std::size_t id;
};

using WokerJobQueue = Queue<&WorkerJob::next>;

// TODO better implementation
struct CoroJob : WorkerJob {
  CoroJob(std::coroutine_handle<> handle) noexcept : handle(handle), WorkerJob(&CoroJob::run) {}
  static auto run(WorkerJob* job) noexcept -> void
  {
    auto coroJob = static_cast<CoroJob*>(job);
    coroJob->handle.resume();
    delete coroJob;
  }
  std::coroutine_handle<> handle;
};
} // namespace coco