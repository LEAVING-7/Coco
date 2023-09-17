#pragma once

#include "coco/util/queue.hpp"

#include <coroutine>

namespace coco {
inline auto genJobId() -> std::size_t
{
  static std::atomic_size_t id = 0;
  return id.fetch_add(1, std::memory_order_relaxed);
}

enum class JobState : std::uint16_t {
  Ready,
  Executing,
  Pending,
  Final,
};

struct WorkerJob {
  using WorkerFn = void (*)(WorkerJob* task, void* args) noexcept;
  WorkerJob(WorkerFn fn, std::atomic<JobState>* state) noexcept : run(fn), next(nullptr), state(state) {}

  WorkerFn run;
  WorkerJob* next;
  std::atomic<JobState>* state;
};

using WorkerJobQueue = util::Queue<&WorkerJob::next>;

inline auto runJob(WorkerJob* job, void* args) noexcept -> void
{
  assert(job != nullptr && "job should not be nullptr");
  job->run(job, args);
}

inline auto emptyFn(WorkerJob*, void*) noexcept -> void { assert(false && "empty job should not be executed"); }
inline WorkerJob emptyJob{emptyFn, nullptr};
inline WorkerJob detachJob{emptyFn, nullptr};

inline std::atomic<JobState> multishotAccept{JobState::Ready};
inline std::atomic<JobState> multishotNofiy{JobState::Ready};

template <typename T = void>
struct Task;

struct ExeOpt {
  std::uint16_t mTid;

  enum Opt : std::uint8_t {
    Balance,
    PreferInOne,
    ForceInOne,
  } mOpt = Balance;

  enum Pri : std::uint8_t {
    Low,
    High,
  } mPri = Low;

  constexpr static auto create(std::uint16_t tid, Opt opt, Pri pri) noexcept -> ExeOpt
  {
    return {.mTid = tid, .mOpt = opt, .mPri = pri};
  }
  constexpr static auto prefInOne() noexcept -> ExeOpt { return {.mTid = 0, .mOpt = PreferInOne, .mPri = Low}; }
  constexpr static auto balance() noexcept -> ExeOpt { return {.mTid = 0, .mOpt = Balance, .mPri = Low}; }
};

class Executor {
public:
  Executor() = default;
  virtual ~Executor() noexcept = default;

  virtual auto execute(WorkerJobQueue&& queue, std::size_t count, ExeOpt opt) noexcept -> void = 0;
  virtual auto execute(WorkerJob* handle, ExeOpt opt) noexcept -> void = 0;
  virtual auto runMain(Task<> task) -> void = 0;
};

} // namespace coco