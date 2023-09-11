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
      if (mMainTaskState->load() == JobState::Final && mTaskQueue.empty()) {
        mState = State::Stop;
      } else {
        mState = State::Waiting;
      }
      mState = State::Executing;
    } else if (currState == State::Executing) {
      processTasks();
      if (mMainTaskState->load() == JobState::Final && mTaskQueue.empty()) {
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
    runJob(job, nullptr);
  }
  if (mMainTaskState->load() == JobState::Final) {
    mState = State::Stop;
  }
}
auto InlExecutor::execute(WorkerJobQueue&& queue, std::size_t count, ExeOpt opt) noexcept -> void
{
  mTaskQueue.append(std::move(queue));
}
auto InlExecutor::execute(WorkerJob* handle, ExeOpt opt) noexcept -> void { mTaskQueue.pushBack(handle); }
auto InlExecutor::runMain(Task<> task) -> void
{
  Proactor::get().attachExecutor(this);
  mMainTaskState = task.promise().getState();
  execute(task.promise().getThisJob());
  processTasks();
  loop();
}
} // namespace coco