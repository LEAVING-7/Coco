#pragma once
#include "coco/__preclude.hpp"

#include "coco/inl_executor.hpp"
#include "coco/mt_executor.hpp"

#include <functional>

namespace coco {
enum class RuntimeType {
  Inline,
  Multi,
};
constexpr static RuntimeType MT = RuntimeType::Multi;
constexpr static RuntimeType INL = RuntimeType::Inline;

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

  struct AnyJobData {
    std::atomic<WorkerJob*> mNextJob;
    int padding[50];
  };

  struct [[nodiscard]] AnyJob : WorkerJob {
    AnyJob(std::shared_ptr<AnyJobData> data)
        : mData(std::move(data)), mExpected(data->mNextJob.load()), WorkerJob(&AnyJob::run)
    {
    }
    static auto run(WorkerJob* job, void*) noexcept -> void
    {
      auto anyJob = static_cast<AnyJob*>(job);
      auto expected = anyJob->mExpected;
      auto& nextJob = anyJob->mData->mNextJob;
      if (nextJob.compare_exchange_strong(expected, &emptyJob)) {
        ::puts("acquire next job success");
        Proactor::get().execute(expected); // acquire next job success
      }
      delete anyJob;
    }
    WorkerJob* mExpected;
    std::shared_ptr<AnyJobData> mData;
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
      auto nextJob = handle.promise().getThisJob();

      auto setNextJob = [&](auto&& task) {
        auto job = new CondJob(&mWaitingCount, nextJob);
        job->action.store(JobAction::OneShot, std::memory_order_relaxed);
        task.promise().setNextJob(job);
        task.promise().setState(JobState::Ready);
      };
      std::apply([&](auto&&... tuple) { (setNextJob(tuple), ...); }, mTasks);

      auto runTask = [this](auto&& task) { mExecutor->enqueue(task.promise().getThisJob()); };
      std::apply([&](auto&&... tuple) { (runTask(tuple), ...); }, mTasks);
    }
    auto await_resume() const noexcept -> void {}

    std::atomic_size_t mWaitingCount;
    Executor* mExecutor;
    TaskTupleTy mTasks;
  };

  template <typename TaskTupleTy>
  struct [[nodiscard]] WaitAnyAwaiter {
    constexpr WaitAnyAwaiter(Executor* executor, TaskTupleTy&& tasks) : mExecutor(executor), mTasks(std::move(tasks)) {}

    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) -> void
    {
      auto nextJob = handle.promise().getThisJob();
      mAnyData = std::make_shared<AnyJobData>(nextJob);
      auto setupJobs = [&](auto&& task) {
        auto job = new AnyJob(mAnyData);
        job->action.store(JobAction::OneShot, std::memory_order_relaxed);
        task.promise().setNextJob(job);
        task.promise().setState(JobState::Ready);
      };
      std::apply([&](auto&&... tuple) { (setupJobs(tuple), ...); }, mTasks);
      auto runTask = [this](auto&& task) { mExecutor->enqueue(task.promise().getThisJob()); };
      std::apply([&](auto&&... tuple) { (runTask(tuple), ...); }, mTasks);
    }

    auto await_resume() noexcept -> void
    {
      // try to do cancelation
      auto doCancel = [&](auto&& task) mutable {
        auto& state = task.promise().getState();
        auto& action = task.promise().getAction();

        JobState expected = JobState::Ready;
        if (!state.compare_exchange_strong(expected, JobState::Cancel)) {
          // cancel failed
          if (expected == JobState::Executing) {
            JobAction expected = JobAction::None;
            if (action.compare_exchange_strong(expected, JobAction::Detach)) {
              [[maybe_unused]] auto handle = task.take(); // give up ownership
            } else if (action == JobAction::Final) {
              // do nothing, task can be destroyed safely
            }
          }
        } else if (expected != JobState::Final) {
          // cancel success, canceled task won't be executed
          ::printf("cancel job %p id = %u\n", task.promise().getThisJob(), task.promise().getState().load());
        }
      };
      std::apply([&](auto&&... tuple) { (doCancel(tuple), ...); }, mTasks);
    }
    std::shared_ptr<AnyJobData> mAnyData;
    Executor* mExecutor;
    TaskTupleTy mTasks;
  };

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

public:
  constexpr Runtime(RuntimeType type, std::size_t threadNum = 0) : mType(type)
  {
    if (type == RuntimeType::Inline) {
      mExecutor = std::make_shared<InlExecutor>();
    } else if (type == RuntimeType::Multi) {
      mExecutor = std::make_shared<MtExecutor>(threadNum);
    }
  }

  auto block(Task<> task) -> void { mExecutor->runMain(std::move(task)); }

  template <typename Rep, typename Period>
  [[nodiscard]] auto sleepFor(std::chrono::duration<Rep, Period> duration) -> Task<>
  {
    auto sleepTime = std::chrono::duration_cast<coco::Duration>(duration);
    co_return co_await SleepAwaiter(sleepTime);
  }

  template <typename Rep, typename Period>
  [[nodiscard]] auto
  sleepUntil(std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<Rep, Period>> time) -> Task<>
  {
    auto sleepTime = std::chrono::duration_cast<coco::Duration>(time - std::chrono::steady_clock::now());
    co_return co_await SleepAwaiter(sleepTime);
  }

  template <typename TaskTy>
  struct JoinHandle {
    JoinHandle() noexcept = default;
    JoinHandle(TaskTy&& task) noexcept
        : mTask(std::forward<TaskTy>(task)), mDone(std::make_unique<std::atomic<WorkerJob*>>(&emptyJob))
    {
      mTask.promise().setListening(this->mDone.get());
      Proactor::get().execute(mTask.promise().getThisJob());
    }
    JoinHandle(JoinHandle&& other) noexcept : mDone(std::move(other.mDone)), mTask(std::move(other.mTask)){};
    auto operator=(JoinHandle&& other) noexcept -> JoinHandle& = default;

    ~JoinHandle() noexcept
    {
      if (mDone != nullptr && mTask.handle() != nullptr && mDone->load() == &emptyJob) {
        assert(false && "looks like you forget to call join()");
      }
    }
    [[nodiscard]] auto join() { return JoinAwaiter(mDone.get()); }

    std::unique_ptr<std::atomic<WorkerJob*>> mDone;
    TaskTy mTask;
  };

  template <TaskConcept... TasksTy>
  [[nodiscard]] constexpr auto waitAll(TasksTy&&... tasks) -> decltype(auto)
  {
    auto tasksTuple = std::make_tuple(std::forward<TasksTy>(tasks)...);
    constexpr auto taskCount = std::tuple_size_v<decltype(tasksTuple)>;
    return WaitNAwaiter<std::tuple<TasksTy...>>(taskCount, mExecutor.get(), std::move(tasksTuple));
  }

  template <typename... JoinHandles>
  [[nodiscard]] auto waitAll(JoinHandles&&... handles) -> Task<>
  {
    if constexpr (sizeof...(handles) == 0) {
      co_return;
    } else if constexpr (requires { std::span(handles...); }) { // span like
      auto span = std::span(std::forward<JoinHandles>(handles)...);
      for (auto& handle : span) {
        co_await handle.join();
      }
    } else {
      (co_await handles.join(), ...);
    }
    co_return;
  }

  template <typename TasksTuple>
  [[nodiscard]] constexpr auto waitN(std::size_t n, TasksTuple&& tuple) -> decltype(auto)
  {
    static_assert(n <= std::tuple_size_v<TasksTuple>, "n must be less than the task count");
    return WaitNAwaiter<TasksTuple>(n, mExecutor.get(), std::move(tuple));
  }

  // FIXME: this function is not correct
  template <TaskConcept... TasksTy>
  [[deprecated("function incorrect")]] constexpr auto waitAny(TasksTy&&... tasks) -> decltype(auto)
  {
    auto tasksTuple = std::make_tuple(std::forward<TasksTy>(tasks)...);
    return WaitAnyAwaiter<std::tuple<TasksTy...>>(mExecutor.get(), std::move(tasksTuple));
  }

  template <TaskConcept TaskTy>
  [[nodiscard]] constexpr auto spawn(TaskTy&& task) -> JoinHandle<TaskTy>
  {
    return JoinHandle<TaskTy>(std::move(task));
  }

  // !!! task must be done before block() function returns, otherwise it will cause memory leak
  template <TaskConcept TaskTy>
  constexpr auto spawnDetach(TaskTy&& task) -> void
  {
    auto& promise = task.promise();
    promise.setAction(JobAction::Detach);
    Proactor::get().execute(promise.getThisJob());
    [[maybe_unused]] auto handel = task.take();
  }

private:
  const RuntimeType mType;
  std::shared_ptr<Executor> mExecutor;
};

template <typename TaskTy>
using JoinHandle = typename Runtime::template JoinHandle<TaskTy>;
} // namespace coco
