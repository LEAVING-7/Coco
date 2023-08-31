#include "mt_executor.hpp"
#include <chrono>
#include <cstdio>
#include <thread>

/* std::atomic_int gCount = 0;
struct Taskkkk : coco::WorkerJob {
  Taskkkk(std::latch& latch) : latch(latch), WorkerJob(nullptr, true)
  {
    run = [](WorkerJob* task) noexcept {
      auto t = static_cast<Taskkkk*>(task);
      gCount.fetch_add(1);
      if (gCount % 100 == 0) {
        putchar('.');
        // printf("%llX: %d\n", std::this_thread::get_id(), gCount.load());
      }
      auto& latch = t->latch;
      delete t;
      latch.count_down();
    };
  }
  std::latch& latch;
};

int main()
{
  auto now = std::chrono::steady_clock::now();
  auto exe = coco::MtExecutor(8);
#define N (1'000'000'000)
  std::latch latch2(N);
  for (int i = 0; i < N; i++) {
    exe.enqueue(new Taskkkk(latch2));
  }
  latch2.wait();
  auto end = std::chrono::steady_clock::now();
  printf("time: %f\n", std::chrono::duration<double>(end - now).count());
  return 0;
} */

#include "runtime.hpp"
using namespace coco;
auto runtime = Runtime(1);

auto taskA() -> Task<int>
{
  ::puts("taskA");
  co_return 233;
}

auto taskC() -> Task<>
{
  co_await runtime.sleep(1s);
  ::puts("taskC");
  co_return;
}
auto taskB() -> Task<double>
{
  auto now = std::chrono::steady_clock::now();
  for (int i = 0; i < 1'000'000; i++) {
    co_await taskA();
  };
  auto end = std::chrono::steady_clock::now();
  printf("time: %f\n", std::chrono::duration<double>(end - now).count());
  // 39.761779s for now
  co_return 1.233;
}
auto main() -> int
{
  auto k = runtime.block([&]() -> Task<int> {
    co_await runtime.waitAll(taskA(), taskC());
    // double b = co_await taskB();
    // printf("%d %f\n", a, b);
    puts("--hello world");
    co_return 233;
  });
  printf("%d\n", k);
}
