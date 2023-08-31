#pragma once
#include "preclude.hpp"

#include "mt_executor.hpp"
#include <functional>

namespace coco {
class Runtime {
  struct SleepAwaiter {
    SleepAwaiter(Duration duration) : mDuration(duration), mJob(nullptr, &CoroJob::runNoDelete) {}
    auto await_ready() const noexcept -> bool { return false; }
    auto await_suspend(std::coroutine_handle<> handle) noexcept -> void
    {
      mJob.handle = handle;
      Proactor::get().addTimer(std::chrono::steady_clock::now() + mDuration, &mJob);
    }
    auto await_resume() const noexcept -> void {}

  private:
    CoroJob mJob;
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
    CoroJob job(task.handle(), &CoroJob::runNoDelete);
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
  [[nodiscard]] auto sleep(std::chrono::duration<Rep, Per> duration) -> SleepAwaiter
  {
    auto sleepTime = std::chrono::duration_cast<coco::Duration>(duration);
    return SleepAwaiter(sleepTime);
  }

  [[nodiscard]] auto waitAll(Task<int> taska, Task<> taskb) -> Task<>
  {
    struct WaitAllAwaiter {
      WaitAllAwaiter(Task<int> taska, Task<> taskb, Executor* exe)
          : mTa(std::move(taska)), mTb(std::move(taskb)), mJob({}, {}), mExecutor(exe)
      {
      }
      auto await_ready() const noexcept -> bool { return false; }
      auto await_suspend(std::coroutine_handle<> handle) noexcept -> void
      {
        mJob.mHandle = handle;
        mJob.mConditions.push_back(&mTa.promise().state);
        mJob.mConditions.push_back(&mTb.promise().state);
        mTa.promise().waitingLists.pushBack(&mJob);
        mTb.promise().waitingLists.pushBack(&mJob);

        auto ha = mTa.handle();
        auto hb = mTb.handle();

        mExecutor->enqueue(new CoroJob(ha, &CoroJob::runDelete));
        mExecutor->enqueue(new CoroJob(hb, &CoroJob::runDelete));
      }
      auto await_resume() const noexcept -> void {}

    private:
      Task<int> mTa;
      Task<> mTb;
      Executor* mExecutor;
      CondJob mJob;
    };
    co_await WaitAllAwaiter(std::move(taska), std::move(taskb), mExecutor.get());
    co_return;
  }

  template <TaskConcept... Tasks>
  struct WaitAllAwaiter : std::tuple<Tasks...> {
    WaitAllAwaiter(Executor* executor) : mJob({}, {}), mExecutor(executor) {}
    auto await_ready() const noexcept -> bool { return false; }
    auto await_suspend(std::coroutine_handle<> handle) noexcept -> void
    {
      mJob.mHandle = handle;
      auto setupTask = [this](auto& task) {
        mJob.mConditions.push_back(&task.promise().state);
        task.promise().waitingLists.pushBack(&mJob);
      };
      std::apply([](auto const&... tuple) { (perTask(tuple), ...); }, *this);
      auto runTask = [this](auto& task) { mExecutor->enqueue(new CoroJob(task.handle(), &CoroJob::runDelete)); };
      std::apply([](auto const&... tuple) { (runTask(tuple), ...); }, *this);
    }
    auto await_resume() const noexcept -> void {}
    CondJob mJob;
    Executor* mExecutor;
  };

  template <typename T>
  struct WaitAllAwaiter2 {
    template <typename... Tasks>
    WaitAllAwaiter2(Executor* executor, Tasks&&... tasks)
        : mTasks(std::forward_as_tuple(tasks...)), mJob({}, {}), mExecutor(executor)
    {
    }

    T mTasks;
    CondJob mJob;
    Executor* mExecutor;
  };
  auto await_ready() const noexcept -> bool { return false; }
  auto await_suspend(std::coroutine_handle<> handle) noexcept -> void {

  }

  auto await_resume() const noexcept -> void {}
  template <TaskConcept... Args>
  [[nodiscard]] auto waitAll(Args&&... args) -> Task<>
  {
    // co_return co_await WaitAllAwaiter(, mExecutor.get());
  }

private:
  std::shared_ptr<MtExecutor> mExecutor;
};
} // namespace coco