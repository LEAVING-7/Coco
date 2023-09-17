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

auto Worker::pushTask(WorkerJob* job, ExeOpt opt) -> void
{
  mQueueMt.lock();
  if (opt.mPri == ExeOpt::High) {
    mTaskQueue.pushFront(job);
  } else {
    mTaskQueue.pushBack(job);
  }
  mQueueMt.unlock();
  notify();
}

auto Worker::tryPushTask(WorkerJob* job, ExeOpt opt) -> bool
{
  std::unique_lock lk(mQueueMt, std::try_to_lock);
  if (!lk.owns_lock()) {
    return false;
  };
  if (opt.mPri == ExeOpt::High) {
    mTaskQueue.pushFront(job);
  } else {
    mTaskQueue.pushBack(job);
  }
  notify();
  return true;
}
auto Worker::pushTask(WorkerJobQueue&& jobs, ExeOpt opt) -> void
{
  assert(jobs.back() == nullptr);
  mQueueMt.lock();
  if (opt.mPri == ExeOpt::High) {
    mTaskQueue.prepend(std::move(jobs));
  } else {
    mTaskQueue.append(std::move(jobs));
  }
  mQueueMt.unlock();
  notify();
}
auto Worker::tryPushTask(WorkerJobQueue&& jobs, ExeOpt opt) -> bool
{
  std::unique_lock lk(mQueueMt, std::try_to_lock);
  if (!lk.owns_lock()) {
    return false;
  }
  if (opt.mPri == ExeOpt::High) {
    mTaskQueue.prepend(std::move(jobs));
  } else {
    mTaskQueue.append(std::move(jobs));
  }
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
        Proactor::get().attachExecutor(this, i);
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
  if (opt.mOpt == ExeOpt::PreferInOne) {
    auto b = mWorkers[opt.mTid]->tryEnqeue(job, opt);
    if (b) {
      return;
    }
  }
  balanceEnqueue(job, opt);
}
auto MtExecutor::execute(WorkerJobQueue&& queue, std::size_t count, ExeOpt opt) noexcept -> void
{
  if (opt.mOpt == ExeOpt::PreferInOne) {
    auto b = mWorkers[opt.mTid]->tryEnqeue(std::move(queue), opt);
    if (b) {
      return;
    }
  }

  if (count == 0) {
    balanceEnqueue(std::move(queue), opt);
  } else {
    while (!queue.empty()) {
      auto const perThread = count / mThreadCount;
      auto perThreadJobs = queue.popFront(perThread);
      balanceEnqueue(std::move(perThreadJobs), opt);
    }
  }
}
auto MtExecutor::runMain(Task<> task) -> void
{
  auto& promise = task.promise();
  auto taskState = std::atomic<JobState>(JobState::Ready);
  promise.setState(&taskState);
  this->execute(promise.getThisJob(), ExeOpt{.mOpt = ExeOpt::ForceInOne, .mPri = ExeOpt::High});
  while (taskState.load() != JobState::Final) {
    taskState.wait(JobState::Ready);
  }
};

} // namespace coco