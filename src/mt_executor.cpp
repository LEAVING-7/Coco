#include "coco/mt_executor.hpp"

namespace coco {
auto Worker::forceStop() -> void
{
  auto state = mState.load(std::memory_order_acquire);
  switch (state) {
  case State::Waiting: {
    notify();
    auto r = mState.compare_exchange_strong(state, State::Stop, std::memory_order_acq_rel);
  } break;
  case State::Executing: {
    auto r = mState.compare_exchange_strong(state, State::Stop, std::memory_order_acq_rel);
  } break;
  case State::Stop: {
    processTasks();
    return;
  } break;
  }
}
auto Worker::start(std::latch& latch) -> void
{
  mProactor = &Proactor::get();
  mState = State::Waiting;
  latch.count_down();
}
auto Worker::loop() -> void
{
  while (true) {
    auto currState = mState.load(std::memory_order_relaxed);
    if (currState == State::Waiting) {
      mProactor->wait();
      auto r = mState.compare_exchange_strong(currState, State::Executing, std::memory_order_acq_rel);
      if (r == false) { // must be stop
        return;
      }
    } else if (currState == State::Executing) {
      processTasks();
      auto r = mState.compare_exchange_strong(currState, State::Waiting, std::memory_order_acq_rel);
      if (r == false) {
        return;
      }
    } else if (currState == State::Stop) [[unlikely]] {
      return;
    } else {
      assert(false);
    }
  }
}
auto Worker::notify() -> void { mProactor->notify(); }
auto Worker::processTasks() -> void
{
  mQueueMt.lock();
  auto jobs = std::move(mTaskQueue);
  mQueueMt.unlock();
  while (auto job = jobs.popFront()) {
    runJob(job, nullptr);
  }
}
auto Worker::pushTask(WorkerJob* job) -> void
{
  mQueueMt.lock();
  mTaskQueue.pushBack(job);
  mQueueMt.unlock();
  notify();
}
auto Worker::tryPushTask(WorkerJob* job) -> bool
{
  std::unique_lock lk(mQueueMt, std::try_to_lock);
  if (!lk.owns_lock()) {
    return false;
  };
  mTaskQueue.pushBack(job);
  notify();
  return true;
}
auto Worker::pushTask(WorkerJobQueue&& jobs) -> void
{
  assert(jobs.back() == nullptr);
  mQueueMt.lock();
  mTaskQueue.append(std::move(jobs));
  mQueueMt.unlock();
  notify();
}
auto Worker::tryPushTask(WorkerJobQueue&& jobs) -> bool
{
  std::unique_lock lk(mQueueMt, std::try_to_lock);
  if (!lk.owns_lock()) {
    return false;
  }
  mTaskQueue.append(std::move(jobs));
  notify();
  return true;
}

// MultiThread executor

MtExecutor::MtExecutor(std::size_t threadCount) : mThreadCount(threadCount)
{
  mWorkers.reserve(threadCount);
  mThreads.reserve(threadCount);
  try {
    for (int i = 0; i < threadCount; i++) {
      mWorkers.emplace_back(std::make_unique<Worker>());
    }
    auto finishLatch = std::latch(threadCount);
    for (int i = 0; i < threadCount; i++) {
      mThreads.emplace_back([this, i, &finishLatch] {
        mWorkers[i]->start(finishLatch);
        Proactor::get().attachExecutor(this);
        mWorkers[i]->loop();
      });
    }
    finishLatch.wait();
  } catch (...) {
    requestStop();
    join();
    throw;
  }
}

auto MtExecutor::requestStop() noexcept -> void
{
  for (auto& worker : mWorkers) {
    worker->forceStop();
  }
}

auto MtExecutor::join() noexcept -> void
{
  for (auto& thread : mThreads) {
    thread.join();
  }
}

auto MtExecutor::execute(WorkerJob* job, ExeOpt opt) noexcept -> void
{
  if (opt == ExeOpt::Balance) {
    balanceEnqueue(job, true);
  } else if (opt == ExeOpt::PreferInOne) {
    balanceEnqueue(job, false);
  }
}
auto MtExecutor::execute(WorkerJobQueue&& queue, std::size_t count, ExeOpt opt) noexcept -> void
{
  if (count == 0) {
    balanceEnqueue(std::move(queue), true);
  } else {
    while (!queue.empty()) {
      auto const perThread = count / mThreadCount;
      auto perThreadJobs = queue.popFront(perThread);
      balanceEnqueue(std::move(perThreadJobs), true);
    }
  }
}
auto MtExecutor::runMain(Task<> task) -> void
{
  auto& action = task.promise().getAction();
  action = JobAction::NotifyAction;
  this->execute(task.promise().getThisJob());
  while (action != JobAction::Final) {
    action.wait(JobAction::NotifyAction);
  }
};

} // namespace coco