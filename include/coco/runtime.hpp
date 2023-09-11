#pragma once
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
  constexpr Runtime(RuntimeKind type, std::size_t threadNum = 0) : mType(type)
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

  template <TaskConcept TaskTy>
  [[nodiscard]] constexpr auto spawn(TaskTy&& task) -> JoinHandle<TaskTy>
  {
    return JoinHandle<TaskTy>(std::move(task));
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
    SleepAwaiter(Duration duration) : mDuration(duration) {}

    auto await_ready() const noexcept -> bool { return false; }
    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> handle) noexcept -> void
    {
      auto job = handle.promise().getThisJob();
      mPromise = &handle.promise();

      Proactor::get().addTimer(std::chrono::steady_clock::now() + mDuration, job);
    }
    auto await_resume() const noexcept -> void {}

  private:
    PromiseBase* mPromise;
    Duration mDuration;
  };
  auto sleepFor(Duration duration) -> Task<> { co_return co_await SleepAwaiter(duration); }

private:
  const RuntimeKind mType;
  std::shared_ptr<Executor> mExecutor;
};
template <typename TaskTy>
using JoinHandle = Runtime::JoinHandle<TaskTy>;
}; // namespace coco