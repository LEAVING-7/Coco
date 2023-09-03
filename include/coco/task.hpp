#pragma once
#include "coco/__preclude.hpp"

#include "coco/proactor.hpp"

#include <coroutine>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace coco {
struct PromiseBase {
  struct CoroJob : WorkerJob {
    CoroJob(std::coroutine_handle<> handle, WorkerJob::fn_type fn) noexcept : handle(handle), WorkerJob(fn) {}
    static auto run(WorkerJob* job, void* args) noexcept -> void
    {
      auto coroJob = static_cast<CoroJob*>(job);
      coroJob->handle.resume();
    }
    std::coroutine_handle<> handle;
  };

  struct FinalAwaiter {
    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
    {
      assert(handle.done() && "handle should done here");
      auto& promise = handle.promise();

      auto next = promise.mNextJob.exchange(nullptr);
      if (next != &emptyJob && next != nullptr) {
        Proactor::get().execute(next);
      }

      auto lastState = promise.getState().exchange(JobState::Final);
      assert(lastState != JobState::Cancel && "canceled job should not be executed");

      auto lastAction = promise.getAction().exchange(JobAction::Final);
      if (lastAction == JobAction::Detach) {
        handle.destroy();
      } else if (lastAction == JobAction::NotifyState) {
        promise.getState().notify_one();
      } else if (lastAction == JobAction::NotifyAction) {
        promise.getAction().notify_one();
      }
    }
    auto await_resume() noexcept -> void {}
  };
  auto initial_suspend() noexcept -> std::suspend_always { return {}; }
  auto final_suspend() noexcept -> FinalAwaiter { return {}; }
  auto unhandled_exception() noexcept -> void { mExceptionPtr = std::current_exception(); }

  auto setCoHandle(std::coroutine_handle<> handle) noexcept -> void { mThisJob.handle = handle; }
  auto getThisJob() noexcept -> WorkerJob* { return &mThisJob; }

  auto setNextJob(WorkerJob* next) noexcept -> void { mNextJob = next; }
  auto getNextJob() noexcept -> std::atomic<WorkerJob*>& { return mNextJob; }

  auto setState(JobState state) -> void { mThisJob.state.store(state); }
  auto getState() noexcept -> std::atomic<JobState>& { return mThisJob.state; }

  auto setAction(JobAction action) -> void { mThisJob.action = action; }
  auto getAction() noexcept -> std::atomic<JobAction>& { return mThisJob.action; }

  CoroJob mThisJob{nullptr, &CoroJob::run};
  std::atomic<WorkerJob*> mNextJob{nullptr};
  std::exception_ptr mExceptionPtr;
};

template <typename T>
struct Promise final : PromiseBase {
  auto get_return_object() noexcept -> Task<T>;
  auto return_value(T value) noexcept -> void { std::construct_at(std::addressof(mRetVal), std::move(value)); }
  auto result() const& -> T const&
  {
    if (mExceptionPtr) {
      std::rethrow_exception(mExceptionPtr);
    }
    return mRetVal;
  }
  auto result() && -> T&&
  {
    if (mExceptionPtr) {
      std::rethrow_exception(mExceptionPtr);
    }
    return std::move(mRetVal);
  }

  T mRetVal;
};

template <>
struct Promise<void> : PromiseBase {
  auto get_return_object() noexcept -> Task<void>;
  auto return_void() noexcept -> void {}
  auto result() const -> void
  {
    if (mExceptionPtr) {
      std::rethrow_exception(mExceptionPtr);
    }
    return;
  }
};

template <typename T>
constexpr bool IsTask = false;

template <typename T>
constexpr bool IsTask<Task<T>> = true;

template <typename T>
concept TaskConcept = IsTask<T>;

template <typename T>
class Task {
public:
  using promise_type = Promise<T>;
  using coroutine_handle_type = std::coroutine_handle<promise_type>;
  using value_type = T;

  Task() noexcept = default;
  explicit Task(coroutine_handle_type handle) noexcept : mHandle(handle)
  {
    assert(mHandle != nullptr);
    mHandle.promise().setCoHandle(mHandle);
  }
  Task(Task&& other) noexcept : mHandle(std::exchange(other.mHandle, nullptr)) {}
  auto operator=(Task&& other) -> Task&
  {
    assert(mHandle == nullptr);
    mHandle = std::exchange(other.mHandle, nullptr);
    return *this;
  };
  constexpr ~Task() noexcept { destroy(); }

  auto operator==(Task const& other) -> bool { return mHandle == other.mHandle; }
  auto done() const noexcept -> bool { return mHandle.done(); }
  auto handle() const noexcept -> coroutine_handle_type { return mHandle; }
  auto resume() const -> bool
  {
    if (mHandle != nullptr && !mHandle.done()) {
      mHandle.resume();
      return true;
    }
    return false;
  }
  auto promise() & -> promise_type& { return mHandle.promise(); }
  auto promise() const& -> promise_type const& { return mHandle.promise(); }
  auto promise() && -> promise_type&& { return std::move(mHandle.promise()); }

  auto take() noexcept -> coroutine_handle_type { return std::exchange(mHandle, nullptr); }

  struct AwaiterBase {
    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
    {
      mHandle.promise().setNextJob(handle.promise().getThisJob());
      Proactor::get().execute(mHandle.promise().getThisJob());
    }
    coroutine_handle_type mHandle;
  };

  auto operator co_await() const& noexcept
  {
    struct Awaiter : AwaiterBase {
      auto await_resume() -> decltype(auto) { return this->mHandle.promise().result(); }
    };
    return Awaiter{mHandle};
  }

  auto operator co_await() && noexcept
  {
    struct Awaiter : AwaiterBase {
      auto await_resume() -> decltype(auto) { return std::move(this->mHandle.promise()).result(); }
    };
    return Awaiter{mHandle};
  }

private:
  auto destroy() -> void
  {
    if (auto handle = std::exchange(mHandle, nullptr); handle != nullptr) {
      // handle.promise().cancel();
      handle.destroy();
    }
  }
  coroutine_handle_type mHandle{nullptr};
};

template <typename T>
inline auto Promise<T>::get_return_object() noexcept -> Task<T>
{
  return Task<T>{std::coroutine_handle<Promise>::from_promise(*this)};
}
inline auto Promise<void>::get_return_object() noexcept -> Task<void>
{
  return Task<void>{std::coroutine_handle<Promise>::from_promise(*this)};
}

struct [[nodiscard]] ThisTask {
  constexpr auto await_ready() const noexcept -> bool { return false; }
  template <typename Promise>
  constexpr auto await_suspend(std::coroutine_handle<Promise> handle) noexcept
  {
    mCoHandle = handle;
    mJobId = handle.promise().getThisJob()->id;
    return false;
  }
  constexpr auto await_resume() const noexcept -> std::pair<std::coroutine_handle<>, std::size_t>
  {
    return {mCoHandle, mJobId};
  }
  std::coroutine_handle<> mCoHandle;
  std::size_t mJobId;
};

struct [[nodiscard]] Yield {
  constexpr auto await_ready() const noexcept -> bool { return false; }
  template <typename Promise>
  constexpr auto await_suspend(std::coroutine_handle<Promise> handle) noexcept
  {
    // suspend then reshcedule
    Proactor::get().execute(handle.promise().getThisJob());
  }
  constexpr auto await_resume() const noexcept -> void {}
};
} // namespace coco
