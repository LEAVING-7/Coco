#pragma once

#include "coco/proactor.hpp"
#include "coco/task.hpp"
#include "coco/worker_job.hpp"

#include <coroutine>

namespace coco {
class InlExecutor : public Executor {
public:
  InlExecutor() : mState(State::Waiting) { mProactor = &Proactor::get(); }
  virtual ~InlExecutor() noexcept { forceStop(); };

  auto execute(WorkerJobQueue&& queue, std::size_t count, ExeOpt opt) noexcept -> void override;
  auto execute(WorkerJob* handle, ExeOpt opt) noexcept -> void override;
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
  std::atomic<JobState> mMainTaskState = JobState::Ready;
};
} // namespace coco