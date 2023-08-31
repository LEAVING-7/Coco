#pragma once
#include "preclude.hpp"

#include "proactor.hpp"

#include <coroutine>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace coco {

template <typename T = void>
struct Task;

enum class PromiseState : std::uint8_t {
  Inital = 0,
  NeedNotifyAtomic = 1,
  NeedNotifyProactor = 2,
  Final = 2,
};

struct CondJob : WorkerJob {
  CondJob(std::atomic_size_t* count, WorkerJob* nextJob) : mCount(count), mNextJob(nextJob), WorkerJob(&CondJob::run) {}

  static auto run(WorkerJob* job) noexcept -> void
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

struct PromiseBase {
  CoroJob currentJob{nullptr, &CoroJob::run};
  WorkerJobQueue waitingLists; // TODO 8 bytes single linked list
  std::exception_ptr exceptionPtr;

  struct FinalAwaiter {
    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
    {
      assert(handle.done() && "handle should done here");
      auto& promise = handle.promise();

      auto state = promise.state.exchange(PromiseState::Final);
      if (state == PromiseState::NeedNotifyAtomic) {
        promise.state.notify_one();
      }

      Proactor::get().execute(std::move(promise.waitingLists));
    }
    auto await_resume() noexcept -> void {}
  };

  auto initial_suspend() noexcept -> std::suspend_always { return {}; }
  auto final_suspend() noexcept -> FinalAwaiter { return {}; }
  auto unhandled_exception() noexcept -> void { exceptionPtr = std::current_exception(); }

  auto setCoHandle(std::coroutine_handle<> handle) noexcept -> void { currentJob.handle = handle; }
  auto addWaitingJob(WorkerJob* job) noexcept -> void { waitingLists.pushBack(job); }
  auto getThisJob() noexcept -> WorkerJob* { return &currentJob; }
};

template <typename T>
struct Promise final : PromiseBase {
  T returnValue;
  std::atomic<PromiseState> state = PromiseState::Inital;

  auto get_return_object() noexcept -> Task<T>;
  auto return_value(T value) noexcept -> void { returnValue = std::move(value); }
  auto result() const& -> T const&
  {
    if (exceptionPtr) {
      std::rethrow_exception(exceptionPtr);
    }
    return returnValue;
  }
  auto result() && -> T&&
  {
    if (exceptionPtr) {
      std::rethrow_exception(exceptionPtr);
    }
    return std::move(returnValue);
  }
};

template <>
struct Promise<void> : PromiseBase {
  std::atomic<PromiseState> state = PromiseState::Inital;

  auto get_return_object() noexcept -> Task<void>;
  auto return_void() noexcept -> void {}
  auto result() const -> void
  {
    if (exceptionPtr) {
      std::rethrow_exception(exceptionPtr);
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
  Task(Task const&) = delete;
  Task(Task&& other) noexcept : mHandle(std::exchange(other.mHandle, nullptr)) {}
  ~Task() noexcept { destroy(); }

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
      mHandle.promise().addWaitingJob(handle.promise().getThisJob());
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
} // namespace coco
