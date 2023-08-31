#pragma once
#include "preclude.hpp"

#include "proactor.hpp"
#include "task.hpp"
#include "timer.hpp"
#include "util/lockfree_queue.hpp"

#include <chrono>
#include <coroutine>
#include <latch>
#include <thread>

using namespace std::chrono_literals;

namespace coco {
class Worker {
public:
  Worker() noexcept = default;
  Worker(Worker const&) = delete;
  Worker(Worker&&) = delete;
  auto operator=(Worker const&) -> Worker& = delete;
  auto operator=(Worker&&) -> Worker& = delete;

  template <typename T>
  [[nodiscard]] auto enqueue(T&& task) noexcept -> bool
  {

    auto state = mState.load(std::memory_order_relaxed);
    if (state == State::Stop) [[unlikely]] {
      return false;
    } else if (state == State::Waiting) {
      pushTask(std::move(task));
      notify();
      return true;
    } else if (state == State::Executing) {
      pushTask(std::move(task));
      return true;
    }
    assert(0);
  }

  template <typename T>
  [[nodiscard]] auto tryEnqeue(T&& task) noexcept -> bool
  {

    auto state = mState.load(std::memory_order_relaxed);
    if (state == State::Stop) [[unlikely]] {
      return false;
    } else if (state == State::Waiting) {

      auto b = tryPushTask(std::move(task));
      if (b) {
        notify();
      }
      return b;
    } else if (state == State::Executing) {
      auto b = tryPushTask(std::move(task));
      if (b) {
        notify();
      }
      return b;
    }
    assert(0);
  }

  auto forceStop() -> void;
  auto start(std::latch& latch) -> void;
  auto loop() -> void;
  auto notify() -> void;

private:
  auto processTasks() -> void;
  auto pushTask(WorkerJob* job) -> void;
  auto tryPushTask(WorkerJob* job) -> bool;

  auto pushTask(WokerJobQueue&& jobs) -> void;
  auto tryPushTask(WokerJobQueue&& jobs) -> bool;

  // private:
  enum class State {
    Waiting,
    Executing,
    Stop,
  };

  coco::Proactor* mProactor = nullptr;
  Queue<&WorkerJob::next> mTaskQueue;
  std::mutex mQueueMt;
  std::atomic<State> mState;
  std::atomic_bool mNofiying = false;
};

class MtExecutor : public Executor {
public:
  MtExecutor(std::size_t threadCount);
  ~MtExecutor() noexcept override
  {
    requestStop();
    join();
  }
  auto requestStop() noexcept -> void;
  auto join() noexcept -> void;
  auto enqueue(std::coroutine_handle<> handle) noexcept -> void override;
  auto enqueue(WokerJobQueue&& queue, std::size_t count) noexcept -> void override;

  template <typename T>
    requires std::is_same_v<WokerJobQueue, T> || std::is_base_of_v<WorkerJob, std::remove_pointer_t<T>>
                                               auto enqueue(T task) noexcept -> void // NOLINT
  {
    auto startIdx = mNextWorker.fetch_add(1, std::memory_order_relaxed) % mThreadCount;
    for (std::uint32_t i = 0; i < mThreadCount; i++) {
      auto const idx = (startIdx + i) < mThreadCount ? (startIdx + i) : (startIdx + i - mThreadCount);
      if (mWorkers[idx]->tryEnqeue(task)) {
        return;
      }
    }
    auto r = mWorkers[startIdx]->enqueue(task);
    assert(r);
  }

private:
  const std::uint32_t mThreadCount;
  std::atomic_uint32_t mNextWorker = 0;
  std::vector<std::thread> mThreads;
  std::vector<std::unique_ptr<Worker>> mWorkers;
};
} // namespace coco
