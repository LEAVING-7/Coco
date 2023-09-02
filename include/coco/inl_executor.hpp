#pragma once
#include "coco/__preclude.hpp"

#include "coco/proactor.hpp"
#include "coco/task.hpp"
#include "coco/worker_job.hpp"

#include <coroutine>

namespace coco {
class InlExecutor : public Executor {
public:
  InlExecutor() : mState(State::Waiting) { mProactor = &Proactor::get(); }
  virtual ~InlExecutor() noexcept { forceStop(); };

  auto enqueue(WorkerJobQueue&& queue, std::size_t count) noexcept -> void override;
  auto enqueue(WorkerJob* handle) noexcept -> void override;
  auto runMain(Task<> task) -> void override;

  auto forceStop() -> void;
  auto loop() -> void;
  auto processTasks() -> void;

private:
  enum class State {
    Waiting,
    Executing,
    Stop,
  };

  Proactor* mProactor = nullptr;
  WorkerJobQueue mTaskQueue;
  State mState;
  std::atomic<PromiseState>* mMainTaskState = nullptr;
};
} // namespace coco