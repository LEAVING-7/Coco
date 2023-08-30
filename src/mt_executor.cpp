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
  mUringInstance = &UringInstance::get();
  mState = State::Waiting;
  latch.count_down();
}
auto Worker::loop() -> void
{
  while (true) {
    auto currState = mState.load(std::memory_order_relaxed);
    switch (currState) {
    case State::Waiting: {
      processTasks();
      io_uring_cqe* cqe = nullptr;
      auto e = mUringInstance->submitWait(cqe, 100s);
      if (e == std::errc::stream_timeout) {
      } else if (e != std::errc(0)) {
        // throw std::system_error(int(e), std::system_category(), "submit and wait failed");
      } else {
        assert(cqe != nullptr);
        if (cqe->flags & IORING_CQE_F_MORE) {
          mNofiying = false;
        } else {
          mTaskQueue.pushFront((WorkerJob*)cqe->user_data);
        }
        mUringInstance->seen(cqe);
      }
      auto r = mState.compare_exchange_strong(currState, State::Executing, std::memory_order_acq_rel);
      if (r == false) { // must be stop
        return;
      }
    } break;
    case State::Executing: {
      processTasks();
      auto r = mState.compare_exchange_strong(currState, State::Waiting, std::memory_order_acq_rel);
      if (r == false) {
        return;
      }
    } break;
    case State::Stop: {
      return;
    }
      [[unlikely]] default : assert(false);
    }
  }
}
auto Worker::notify() -> void
{
  if (mNofiying.exchange(true) == false) {
    mUringInstance->notify();
    return;
  }
}
auto Worker::processTasks() -> void
{
  mQueueMt.lock();
  while (auto task = mTaskQueue.popFront()) {
    mQueueMt.unlock();
    auto fn = task->run;
    fn(task);
    mQueueMt.lock();
  }
  mQueueMt.unlock();
}
auto Worker::pushTask(WorkerJob* job) -> void
{
  mQueueMt.lock();
  mTaskQueue.pushBack(job);
  mQueueMt.unlock();
}
auto Worker::tryPushTask(WorkerJob* job) -> bool
{
  std::unique_lock lk(mQueueMt, std::try_to_lock);
  if (!lk.owns_lock()) {
    return false;
  }
  mTaskQueue.pushBack(job);
  return true;
}

auto Worker::pushTask(WokerJobQueue&& jobs) -> void
{
  mQueueMt.lock();
  mTaskQueue.append(std::move(jobs));
  mQueueMt.unlock();
}

auto Worker::tryPushTask(WokerJobQueue&& jobs) -> bool
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
        UringInstance::get().attachMtExecutor(this);
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

auto MtExecutor::enqueue(std::coroutine_handle<> handle) -> void
{
  auto job = new CoroJob(handle);
  enqueue(job);
}

auto MtExecutor::enqueue(WokerJobQueue&& queue, std::size_t count) -> void
{
  if (count == 0) {
    while (auto job = queue.popFront()) {
      enqueue(job);
    }
  } else {
    while (!queue.empty()) {
      auto const perThread = count / mThreadCount;
      auto perThreadJobs = queue.popFront(perThread);
      enqueue(std::move(perThreadJobs));
    }
  }
}

} // namespace coco