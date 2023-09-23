#pragma once
#include "coco/task.hpp"
// guarantee all the tasks in the chain will be executed in order and in the same thread
namespace coco {
class Runtime;
}
namespace coco::sync {
class Chain {
public:
  Chain() noexcept = default;
  Chain(void* ctx) noexcept : mCtx(ctx){};
  Chain(Chain const&) = delete;
  auto operator=(Chain const&) -> Chain& = delete;
  Chain(Chain&&) = default;
  auto operator=(Chain&&) -> Chain& = default;
  ~Chain() noexcept { assert(mQueue.empty() && "Chain must be empty"); }
  auto withCtx(void* ctx) noexcept -> Chain&
  {
    mCtx = ctx;
    return *this;
  }
  auto chain(Task<> taskFn(void* ctx)) noexcept -> Chain&
  {
    auto task = taskFn(mCtx);
    task.promise().setDetach();
    mQueue.pushBack(task.promise().getThisJob());
    [[maybe_unused]] auto t = task.take();
    return *this;
  }

private:
  friend class ::coco::Runtime;
  auto chain(WorkerJob* job) noexcept -> void { mQueue.pushBack(job); }
  auto takeQueue() noexcept -> WorkerJobQueue { return std::move(mQueue); }

  void* mCtx = nullptr;
  WorkerJobQueue mQueue;
};
} // namespace coco::sync