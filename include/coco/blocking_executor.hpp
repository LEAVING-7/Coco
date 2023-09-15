#pragma once
#include "coco/task.hpp"
#include "coco/util/panic.hpp"

#include <condition_variable>
#include <mutex>

namespace coco {
class BlockingThreadPool {
public:
  BlockingThreadPool(std::size_t threadLimit) : mIdleCount(0), mThreadCount(0), mThreadLimits(threadLimit) {}
  ~BlockingThreadPool() {}
  auto enqueue(WorkerJob* task) -> void
  {
    auto lk = std::unique_lock(mQueueMt);
    mQueue.pushBack(task);
    mQueueSize += 1;
    mQueueCv.notify_one();
    growPool();
  }

private:
  auto loop() -> void
  {
    using namespace std::chrono_literals;
    auto lk = std::unique_lock(mQueueMt);
    while (true) {
      mIdleCount -= 1;
      while (!mQueue.empty()) {
        growPool();
        auto task = mQueue.popFront();
        lk.unlock();
        task->run(task, 0);
        lk.lock();
      }
      mIdleCount += 1;
      auto r = mQueueCv.wait_for(lk, 500ms);
      if (r == std::cv_status::timeout && mQueue.empty()) {
        mIdleCount -= 1;
        mThreadCount -= 1;
        break;
      }
    }
  }

  auto growPool() -> void
  {
    assert(!mQueueMt.try_lock());
    while (mQueueSize > mIdleCount * 5 && mThreadCount < mThreadLimits) {
      mThreadCount += 1;
      mIdleCount += 1;
      mQueueCv.notify_all();
      std::thread([this]() { loop(); }).detach();
    }
  }

private:
  std::mutex mQueueMt;
  std::condition_variable mQueueCv;
  util::Queue<&WorkerJob::next> mQueue;
  std::size_t mQueueSize;

  std::size_t mIdleCount;
  std::size_t mThreadCount;
  std::size_t mThreadLimits;
};

class BlockingExecutor {
public:
  BlockingExecutor(std::size_t threadLimit) : mPool(threadLimit) {}
  ~BlockingExecutor() = default;
  auto execute(WorkerJob* handle, ExeOpt opt) noexcept -> void { mPool.enqueue(handle); }
  BlockingThreadPool mPool;
};
} // namespace coco