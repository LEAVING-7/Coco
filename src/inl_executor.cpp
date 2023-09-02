#include "coco/inl_executor.hpp"

namespace coco {
auto InlExecutor::forceStop() -> void
{
  switch (mState) {
  case State::Waiting:
    mProactor->notify();
    mState = State::Stop;
    break;
  case State::Executing:
    mState = State::Stop;
    break;
  case State::Stop:
    break;
  }
}
auto InlExecutor::loop() -> void
{
  while (true) {
    auto currState = mState;
    if (currState == State::Waiting) {
      mProactor->wait();
      if (mMainTaskState->load() == PromiseState::Final) {
        mState = State::Stop;
      } else {
        mState = State::Waiting;
      }
      mState = State::Executing;
    } else if (currState == State::Executing) {
      processTasks();
      if (mMainTaskState->load() == PromiseState::Final) {
        mState = State::Stop;
      } else {
        mState = State::Waiting;
      }
    } else if (currState == State::Stop) [[unlikely]] {
      return;
    } else {
      assert(false);
    }
  }
}
auto InlExecutor::processTasks() -> void
{
  while (auto job = mTaskQueue.popFront()) {
    job->run(job, nullptr);
  }
  if (mMainTaskState->load() == PromiseState::Final) {
    mState = State::Stop;
  }
}
auto InlExecutor::enqueue(WorkerJobQueue&& queue, std::size_t count) noexcept -> void
{
  mTaskQueue.append(std::move(queue));
}
auto InlExecutor::enqueue(WorkerJob* handle) noexcept -> void { mTaskQueue.pushBack(handle); }
auto InlExecutor::runMain(Task<> task) -> void
{
  Proactor::get().attachExecutor(this);
  mMainTaskState = &task.promise().mState;
  enqueue(task.promise().getThisJob());
  processTasks();
  loop();
}
} // namespace coco