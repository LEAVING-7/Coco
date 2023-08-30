#pragma once
#include "preclude.hpp"

#include "task.hpp"
#include "uring.hpp"
#include "util/lockfree_queue.hpp"

#include <chrono>
#include <coroutine>
#include <latch>
#include <thread>

using namespace std::chrono_literals;

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

class Worker {
public:
  Worker() noexcept = default;
  Worker(Worker const&) = delete;
  Worker(Worker&&) = delete;
  auto operator=(Worker const&) -> Worker& = delete;
  auto operator=(Worker&&) -> Worker& = delete;

  template <bool tryEnqueue = false>
  [[nodiscard]] auto enqueue(WorkerJob* task) noexcept -> bool
  {
    auto state = mState.load(std::memory_order_acquire);
    switch (state) {
    case State::Waiting: {
      notify();
      pushTask(task);
      return true;
    } break;
    case State::Executing: {
      if constexpr (!tryEnqueue) {
        pushTask(task);
        return true;
      } else {
        return tryPushTask(task);
      }
    } break;
    case State::Stop: {
      return false;
    } break;
    }
    return false;
  }

  auto forceStop() -> void;
  auto start(std::latch& latch) -> void;
  auto loop() -> void;
  auto notify() -> void;

private:
  auto processTasks() -> void;
  auto pushTask(WorkerJob* job) -> void;
  auto tryPushTask(WorkerJob* job) -> bool;

private:
  enum class State {
    Waiting,
    Executing,
    Stop,
  };

  coco::UringInstance* mUringInstance = nullptr;
  Queue<&WorkerJob::next> mTaskQueue;
  std::mutex mQueueMt;
  std::atomic<State> mState;
  std::atomic_bool mNofiying = false;
};

class MtExecutor {
public:
  MtExecutor(std::size_t threadCount);
  ~MtExecutor() noexcept
  {
    requestStop();
    join();
  }
  auto requestStop() noexcept -> void;
  auto join() noexcept -> void;
  auto enqueue(WorkerJob* task) noexcept -> void;
  auto enqueue(std::coroutine_handle<> handle) -> void;

private:
  const std::uint32_t mThreadCount;
  std::atomic_uint32_t mNextWorker = 0;
  std::vector<std::thread> mThreads;
  std::vector<std::unique_ptr<Worker>> mWorkers;
};
} // namespace coco
