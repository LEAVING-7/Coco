#pragma once
#include "preclude.hpp"

#include "mt_executor.hpp"
#include <functional>

namespace coco {
class Runtime {

  struct SleepAwaiter {
    SleepAwaiter(Duration duration) : mDuration(duration) {}
    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
    {
      Proactor::get().addTimer(std::chrono::steady_clock::now() + mDuration, handle.promise().getThisJob());
    }
    auto await_resume() const noexcept -> void {}

  private:
    Duration mDuration;
  };

public:
  Runtime(std::size_t threadNum) : mExecutor(std::make_shared<MtExecutor>(threadNum)) {}

  template <typename F, typename... Args>
    requires std::invocable<F, Args...>
  auto block(F&& fn, Args&&... args) -> decltype(auto)
  {
    using result_t = std::invoke_result_t<F, Args...>;
    static_assert(coco::IsTask<result_t>, "block function must return a task");

    auto task = std::invoke(fn, std::forward<Args>(args)...);
    auto& state = task.promise().state;
    CoroJob job(task.handle(), &CoroJob::run);
    state = coco::PromiseState::NeedNotifyAtomic;

    mExecutor.get()->enqueue(&job);
    while (state != coco::PromiseState::Final) {
      ::puts("block on waiting...");
      state.wait(coco::PromiseState::NeedNotifyAtomic);
    }
    auto result = std::move(task.promise()).result();
    return result;
  }

  template <typename Rep, typename Per>
  [[nodiscard]] auto sleep(std::chrono::duration<Rep, Per> duration) -> Task<>
  {
    auto sleepTime = std::chrono::duration_cast<coco::Duration>(duration);
    co_return co_await SleepAwaiter(sleepTime);
  }

  template <typename TaskTupleTy>
  struct WaitAllAwaiter {
    WaitAllAwaiter(Executor* executor, TaskTupleTy&& tasks)
        : mJob({}, {}), mExecutor(executor), mTasks(std::move(tasks))
    {
    }

    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) -> void
    {
      mJob.mHandle = handle;
      auto setupTask = [this](auto&& task) {
        mJob.mConditions.push_back(&task.promise().state);
        task.promise().addWaitingJob(&mJob);
      };
      std::apply([&](auto&&... tuple) { (setupTask(tuple), ...); }, mTasks);
      assert(mJob.mConditions.size() == std::tuple_size_v<TaskTupleTy>);
      auto runTask = [this](auto&& task) { mExecutor->enqueue(task.promise().getThisJob()); };
      std::apply([&](auto&&... tuple) { (runTask(tuple), ...); }, mTasks);
    }
    auto await_resume() const noexcept -> void {}

    Executor* mExecutor;
    TaskTupleTy mTasks;
    CondJob mJob;
  };

  auto await_ready() const noexcept -> bool { return false; }
  auto await_suspend(std::coroutine_handle<> handle) noexcept -> void {}

  auto await_resume() const noexcept -> void {}
  template <TaskConcept... TasksTy>
  [[nodiscard]] auto waitAll(TasksTy&&... tasks) -> Task<>
  {
    auto tasksTuple = std::make_tuple(std::forward<TasksTy>(tasks)...);
    co_return co_await WaitAllAwaiter<std::tuple<TasksTy...>>(mExecutor.get(), std::move(tasksTuple));
  }

private:
  std::shared_ptr<MtExecutor> mExecutor;
};
} // namespace coco