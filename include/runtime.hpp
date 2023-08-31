#pragma once
#include "preclude.hpp"

#include "mt_executor.hpp"
#include <functional>

namespace coco {
class Runtime {
  struct SleepAwaiter {
    SleepAwaiter(Duration duration) : mDuration(duration), mJob(nullptr, false) {}
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
    state = coco::PromiseState::NeedNotify;

    mExecutor.get()->enqueue(task.handle());
    while (state != coco::PromiseState::Final) {
      state.wait(coco::PromiseState::NeedNotify);
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

private:
  std::shared_ptr<MtExecutor> mExecutor;
};
} // namespace coco