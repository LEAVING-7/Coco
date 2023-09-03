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
  struct AnyJobData {
    std::atomic<WorkerJob*> mNextJob;
  };

  struct [[nodiscard]] AnyJob : WorkerJob {
    AnyJob(std::shared_ptr<AnyJobData> data, std::coroutine_handle<> self)
        : mData(std::move(data)), mExpected(data->mNextJob.load()), mSelf(self), WorkerJob(&AnyJob::run)
    {
    }
    static auto run(WorkerJob* job, void*) noexcept -> void
    {
      auto anyJob = static_cast<AnyJob*>(job);
      auto expected = anyJob->mExpected;
      auto& nextJob = anyJob->mData->mNextJob;
      if (nextJob.compare_exchange_strong(expected, &emptyJob)) {
        Proactor::get().execute(expected); // acquire next job success
      }
      anyJob->mSelf.destroy();
      delete anyJob;
    }
    std::coroutine_handle<> mSelf;
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
  struct [[nodiscard]] WaitAnyAwaiter {
    constexpr WaitAnyAwaiter(Executor* executor, TaskTupleTy&& tasks) : mExecutor(executor), mTasks(std::move(tasks)) {}

    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) -> void
    {
      auto nextJob = handle.promise().getThisJob();
      mAnyData = std::make_shared<AnyJobData>(nextJob);
      auto setupJobs = [&](auto&& task) {
        auto job = new AnyJob(mAnyData, nullptr);
        job->action.store(JobAction::OneShot, std::memory_order_relaxed);
        task.promise().setNextJob(job);
        task.promise().setState(JobState::Ready);
        job->mSelf = task.handle();
      };
      std::apply([&](auto&&... tuple) { (setupJobs(tuple), ...); }, mTasks);
      auto runTask = [this](auto&& task) { mExecutor->execute(task.promise().getThisJob()); };
      std::apply([&](auto&&... tuple) { (runTask(tuple), ...); }, mTasks);
      auto takeAll = [this](auto&& task) {
        [[maybe_unused]] auto handle = task.take(); // give up ownership
      };
      std::apply([&](auto&&... tuple) { (takeAll(tuple), ...); }, mTasks);
    }
    auto await_resume() noexcept -> void {}

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
    JoinHandle(TaskTy&& task) noexcept : mTask(std::forward<TaskTy>(task))
    {
      mTask.promise().setNextJob(&emptyJob);
      mDone = &mTask.promise().getNextJob();
      Proactor::get().execute(mTask.promise().getThisJob());
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

    std::atomic<WorkerJob*>* mDone;
    TaskTy mTask;
  };

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

  // not recommend to use
  template <TaskConcept... TasksTy>
  [[nodiscard]] constexpr auto waitAny(TasksTy&&... tasks) -> decltype(auto)
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
