#pragma once
#include "coco/__preclude.hpp"

#include "mt_executor.hpp"
#include <functional>

namespace coco {
class Runtime {
  struct [[nodiscard]] CondJob : WorkerJob {
    CondJob(std::atomic_size_t* count, WorkerJob* nextJob) : mCount(count), mNextJob(nextJob), WorkerJob(&CondJob::run)
    {
    }

    static auto run(WorkerJob* job, void*) noexcept -> void
    {
      auto condJob = static_cast<CondJob*>(job);
      auto count = condJob->mCount->fetch_sub(1);
      if (count == 1) {
        Proactor::get().execute(condJob->mNextJob);
      }
      delete condJob;
    }
    std::atomic_size_t* mCount;
    WorkerJob* mNextJob;
  };

  struct [[nodiscard]] SleepAwaiter {
    SleepAwaiter(Duration duration) : mDuration(duration) {}
    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
    {
      auto job = handle.promise().getThisJob();
      Proactor::get().addTimer(std::chrono::steady_clock::now() + mDuration, job);
    }
    auto await_resume() const noexcept -> void {}

  private:
    Duration mDuration;
  };

public:
  Runtime(std::size_t threadNum) : mExecutor(std::make_shared<MtExecutor>(threadNum)) {}

  template <typename F, typename... Args>
    requires std::invocable<F, Args...>
  auto block(F&& fn, Args&&... args) -> auto
  {
    using result_t = std::invoke_result_t<F, Args...>;
    static_assert(coco::IsTask<result_t>, "block function must return a task");

    auto task = std::invoke(fn, std::forward<Args>(args)...);
    auto& state = task.promise().mState;
    CoroJob job(task.handle(), &CoroJob::run);
    state = coco::PromiseState::NeedNotifyAtomic;

    mExecutor.get()->enqueue(&job);
    while (state != coco::PromiseState::Final) {
      state.wait(coco::PromiseState::NeedNotifyAtomic);
    }
    return std::move(task.promise()).result();
  }

  template <typename Rep, typename Per>
  [[nodiscard]] auto sleep(std::chrono::duration<Rep, Per> duration) -> Task<>
  {
    auto sleepTime = std::chrono::duration_cast<coco::Duration>(duration);
    co_return co_await SleepAwaiter(sleepTime);
  }

  template <typename TaskTupleTy>
  struct [[nodiscard]] WaitNAwaiter {
    constexpr WaitNAwaiter(std::size_t waiting, Executor* executor, TaskTupleTy&& tasks)
        : mWaitingCount(waiting), mExecutor(executor), mTasks(std::move(tasks))
    {
    }

    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) -> void
    {
      std::vector<std::atomic<PromiseState>*> jobConds;
      auto nextJob = handle.promise().getThisJob();

      auto setupTask = [&](auto&& task) { jobConds.push_back(&task.promise().mState); };
      std::apply([&](auto&&... tuple) { (setupTask(tuple), ...); }, mTasks);

      auto addWaitingJob = [&](auto&& task) { task.promise().addWaitingJob(new CondJob(&mWaitingCount, nextJob)); };

      std::apply([&](auto&&... tuple) { (addWaitingJob(tuple), ...); }, mTasks);

      assert(jobConds.size() == std::tuple_size_v<TaskTupleTy>);

      auto runTask = [this](auto&& task) { mExecutor->enqueue(task.promise().getThisJob()); };
      std::apply([&](auto&&... tuple) { (runTask(tuple), ...); }, mTasks);
    }
    auto await_resume() const noexcept -> void {}

    std::atomic_size_t mWaitingCount;
    Executor* mExecutor;
    TaskTupleTy mTasks;
  };

  template <TaskConcept... TasksTy>
  [[nodiscard]] constexpr auto waitAll(TasksTy&&... tasks) -> decltype(auto)
  {
    auto tasksTuple = std::make_tuple(std::forward<TasksTy>(tasks)...);
    constexpr auto taskCount = std::tuple_size_v<decltype(tasksTuple)>;
    return WaitNAwaiter<std::tuple<TasksTy...>>(taskCount, mExecutor.get(), std::move(tasksTuple));
  }

  template <typename TasksTuple>
  [[nodiscard]] constexpr auto waitN(std::size_t n, TasksTuple&& tuple) -> decltype(auto)
  {
    static_assert(n <= std::tuple_size_v<TasksTuple>, "n must be less than the task count");
    return WaitNAwaiter<TasksTuple>(n, mExecutor.get(), std::move(tuple));
  }

  // FIXME: this function is not correct
  template <TaskConcept... TasksTy>
  [[nodiscard]] constexpr auto waitAny(TasksTy&&... tasks) -> decltype(auto)
  {
    auto tasksTuple = std::make_tuple(std::forward<TasksTy>(tasks)...);
    return WaitNAwaiter<std::tuple<TasksTy...>>(1, mExecutor.get(), std::move(tasksTuple));
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
    JoinHandle(TaskTy&& task) noexcept
        : mTask(std::forward<TaskTy>(task)), mDone(std::make_unique<std::atomic<WorkerJob*>>(&emptyJob))
    {
      mTask.promise().addContinueJob(mDone.get());
      Proactor::get().execute(mTask.promise().getThisJob());
    }
    JoinHandle(JoinHandle&&) noexcept = default;
    ~JoinHandle() noexcept
    {
      if (mDone->load() != nullptr) {
        assert("looks like you forget to call join()");
      }
    }
    [[nodiscard]] auto join() -> decltype(auto) { return JoinAwaiter(mDone.get()); }

    // atomic variable cannot be moved, so I use unique_ptr to wrap it
    std::unique_ptr<std::atomic<WorkerJob*>> mDone;
    TaskTy mTask;
  };

  template <TaskConcept TaskTy>
  [[nodiscard]] constexpr auto spawn(TaskTy&& task) -> decltype(auto)
  {
    return JoinHandle<TaskTy>(std::move(task));
  }

  // !!! task must be done before block() function returns, otherwise it will cause memory leak
  template <TaskConcept TaskTy>
  [[nodiscard]] constexpr auto spawnDetach(TaskTy&& task) -> decltype(auto)
  {
    auto& promise = task.promise();
    promise.mState.store(coco::PromiseState::NeedDetach);
    Proactor::get().execute(promise.getThisJob());
    [[maybe_unused]] auto handel = task.take();
  }

private:
  std::shared_ptr<MtExecutor> mExecutor;
};
} // namespace coco
