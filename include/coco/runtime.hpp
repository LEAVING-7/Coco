#pragma once

#include "coco/blocking_executor.hpp"
#include "coco/inl_executor.hpp"
#include "coco/mt_executor.hpp"

namespace coco {
enum class RuntimeKind {
  Inline,
  Multi,
};
constexpr inline RuntimeKind MT = RuntimeKind::Multi;
constexpr inline RuntimeKind INL = RuntimeKind::Inline;
class Runtime {
public:
  constexpr Runtime(RuntimeKind type, std::size_t threadNum = 4) : mBlockingThreadsMax(500), mBlocking(nullptr)
  {
    if (type == RuntimeKind::Inline) {
      mExecutor = std::make_shared<InlExecutor>();
    } else if (type == RuntimeKind::Multi) {
      mExecutor = std::make_shared<MtExecutor>(threadNum);
    }
  }

  struct [[nodiscard]] JoinAwaiter {
    JoinAwaiter(std::atomic<WorkerJob*>* done) : mDone(done) {}
    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> bool
    {
      auto job = handle.promise().getThisJob();
      WorkerJob* expected = &emptyJob;
      if (mDone->compare_exchange_strong(expected, job)) {
        return true;
      } else {
        return false;
      }
    }
    auto await_resume() const noexcept -> void {}

    std::atomic<WorkerJob*>* mDone;
  };
  template <typename TaskTy>
  struct JoinHandle {
    JoinHandle() noexcept = default;
    JoinHandle(TaskTy&& task) noexcept : mTask(std::forward<TaskTy>(task))
    {
      mTask.promise().setNextJob(&emptyJob);
      mDone = &mTask.promise().getNextJob();
      Proactor::get().execute(mTask.promise().getThisJob(), ExeOpt::balance());
    }
    JoinHandle(JoinHandle&& other) noexcept
        : mDone(std::exchange(other.mDone, nullptr)), mTask(std::move(other.mTask)){};
    auto operator=(JoinHandle&& other) noexcept -> JoinHandle& = default;
    ~JoinHandle() noexcept
    {
      if (mDone != nullptr && mTask.handle() != nullptr && mDone->load() == &emptyJob) {
        assert(false && "looks like you forget to call join()");
      }
    }
    [[nodiscard]] auto join() { return JoinAwaiter(mDone); }

    auto result() -> decltype(auto) { return mTask.promise().result(); }

    std::atomic<WorkerJob*>* mDone;
    TaskTy mTask;
  };

  template <TaskConcept TaskTy>
  [[nodiscard]] constexpr auto spawn(TaskTy&& task) -> JoinHandle<TaskTy>
  {
    return JoinHandle<TaskTy>(std::move(task));
  }

  template <typename TaskTy>
  [[nodiscard]] auto waitAll(std::span<JoinHandle<TaskTy>> handles) -> Task<>
  {
    for (auto& handle : handles) {
      co_await handle.join();
    }
    co_return;
  }

  template <typename... JoinHandles>
  [[nodiscard]] auto waitAll(JoinHandles&&... handles) -> Task<>
  {
    if constexpr (sizeof...(handles) == 0) {
      co_return;
    } else {
      (co_await handles.join(), ...);
    }
    co_return;
  }

  template <TaskConcept... TasksTy>
  [[nodiscard]] auto waitAll(TasksTy&&... tasks) -> Task<>
  {
    auto tasksTuple = std::make_tuple(std::forward<TasksTy>(tasks)...);
    constexpr auto taskCount = std::tuple_size_v<decltype(tasksTuple)>;
    auto joinHandles = std::vector<JoinHandle<Task<>>>();
    joinHandles.reserve(taskCount);
    std::apply([&](auto&&... task) { (joinHandles.push_back(spawn(std::move(task))), ...); }, tasksTuple);
    co_return co_await waitAll(joinHandles);
  }

  auto block(Task<> task) -> void { mExecutor->runMain(std::move(task)); }

  struct [[nodiscard]] SleepAwaiter {
    SleepAwaiter(Instant instant) : mInstant(instant) {}

    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
    {
      auto job = handle.promise().getThisJob();
      mPromise = &handle.promise();
      Proactor::get().addTimer(mInstant, job);
    }
    auto await_resume() const noexcept -> void {}

  private:
    PromiseBase* mPromise;
    Instant mInstant;
  };
  template <typename Rep, typename Period>
  auto sleepFor(std::chrono::duration<Rep, Period> duration) -> Task<>
  {
    if (duration.count() == 0) {
      co_return;
    }
    auto now = std::chrono::steady_clock::now();
    co_await SleepAwaiter(now + std::chrono::duration_cast<Duration>(duration));
  }
  auto sleepUntil(Instant time) -> Task<>
  {
    auto now = std::chrono::steady_clock::now();
    if (time <= now) {
      co_return;
    }
    co_await SleepAwaiter(time);
  }

  template <typename FnTy>
  struct BlockOnJob : WorkerJob {
    using result_type = std::invoke_result_t<FnTy>;
    BlockOnJob(Runtime* rt, PromiseBase* next, FnTy&& fn)
        : WorkerJob(run, nullptr), mRuntime(rt), mNextPromise(next), mFn(std::forward<FnTy>(fn))
    {
    }

    static auto run(WorkerJob* job, void* /* arg */) noexcept -> void
    {
      auto self = static_cast<BlockOnJob*>(job);
      try {
        self->mResult = self->mFn();
      } catch (...) {
        self->mNextPromise->setExeception(std::current_exception());
      }
      self->mRuntime->mExecutor.get()->execute(self->mNextPromise->getThisJob(), ExeOpt::balance());
    }
    result_type mResult;
    FnTy mFn;
    Runtime* mRuntime;
    PromiseBase* mNextPromise;
  };

  template <typename Fn>
  struct [[nodiscard]] BlockOnAwaiter {
    using result_type = std::invoke_result_t<Fn>;
    BlockOnAwaiter(Runtime* rt, Fn&& fn) : mJob(rt, nullptr, std::forward<Fn>(fn)) {}

    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
    {
      mJob.mNextPromise = &handle.promise();
      mJob.mRuntime->mBlocking.get()->execute(&mJob);
    }
    auto await_resume() const -> result_type
    {
      if (mJob.mNextPromise->hasException()) {
        std::rethrow_exception(mJob.mNextPromise->currentException());
      }
      return std::move(mJob.mResult);
    }

    BlockOnJob<Fn> mJob;
  };

  // FIXME !! this function is not exception safe, i guess
  template <std::invocable Fn, typename... Args>
  [[nodiscard]] auto blockOn(Fn&& fn, Args... args)
  {
    if (mBlocking == nullptr) {
      std::call_once(mBlockingOnceFlag, [&]() { mBlocking = std::make_shared<BlockingExecutor>(mBlockingThreadsMax); });
    }
    return BlockOnAwaiter{this, [&]() -> decltype(auto) { return fn(args...); }};
  }

  auto spawnDetach(Task<> task) -> void
  {
    task.promise().setNextJob(&detachJob);
    mExecutor.get()->execute(task.promise().getThisJob(), ExeOpt::prefInOne());
    [[maybe_unused]] auto dummy = task.take();
  }

private:
  std::shared_ptr<Executor> mExecutor;
  std::shared_ptr<BlockingExecutor> mBlocking;
  std::once_flag mBlockingOnceFlag;
  std::uint32_t mBlockingThreadsMax;
};
template <typename TaskTy>
using JoinHandle = Runtime::JoinHandle<TaskTy>;
}; // namespace coco