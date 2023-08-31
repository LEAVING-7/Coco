#include "mt_executor.hpp"

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
      auto wakeupJob = mProactor->wait(mNofiying);
      auto r = mState.compare_exchange_strong(currState, State::Executing, std::memory_order_acq_rel);
      if (wakeupJob != nullptr) {
        auto r = enqueue(wakeupJob);
      }
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
auto Worker::notify() -> void
{
  if (mNofiying.exchange(true) == false) {
    mProactor->notify();
    return;
  }
}
auto Worker::processTasks() -> void
{
  mQueueMt.lock();
  auto jobs = std::move(mTaskQueue);
  mQueueMt.unlock();
  // FIXME: receve a job with next point to it self
  while (auto job = jobs.popFront()) {
    job->run(job);
  }
}
auto Worker::pushTask(WorkerJob* job) -> void
{
  mQueueMt.lock();
  assert(job->next == nullptr);
  mTaskQueue.pushBack(job);
  mQueueMt.unlock();
}
auto Worker::tryPushTask(WorkerJob* job) -> bool
{
  std::unique_lock lk(mQueueMt, std::try_to_lock);
  if (!lk.owns_lock()) {
    return false;
  };
  assert(job->next == nullptr);
  mTaskQueue.pushBack(job);
  return true;
}

auto Worker::pushTask(WorkerJobQueue&& jobs) -> void
{
  assert(jobs.back() == nullptr);
  mQueueMt.lock();
  mTaskQueue.append(std::move(jobs));
  mQueueMt.unlock();
}

auto Worker::tryPushTask(WorkerJobQueue&& jobs) -> bool
{
  std::unique_lock lk(mQueueMt, std::try_to_lock);
  if (!lk.owns_lock()) {
    return false;
  }
  mTaskQueue.append(std::move(jobs));
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

auto MtExecutor::enqueue(WorkerJob* job) noexcept -> void { enqueuImpl(job); }

auto MtExecutor::enqueue(WorkerJobQueue&& queue, std::size_t count) noexcept -> void
{
  if (count == 0) {
    enqueuImpl(std::move(queue));
  } else {
    while (!queue.empty()) {
      auto const perThread = count / mThreadCount;
      auto perThreadJobs = queue.popFront(perThread);
      enqueuImpl(std::move(perThreadJobs));
    }
  }
}

} // namespace coco