#include "mt_executor.hpp"
#include <chrono>
#include <cstdio>
#include <thread>
int main() {}
/* std::atomic_int gCount = 0;
struct Taskkkk : WorkerJob {
  Taskkkk(std::latch &latch) : latch(latch) {
    run = [](WorkerJob *task) noexcept {
      auto t = static_cast<Taskkkk *>(task);
      gCount.fetch_add(1);
      if (gCount % 10 == 0) {
        printf("%llX: %d\n", std::this_thread::get_id(), gCount.load());
      }
      std::this_thread::sleep_for(1ms);
      auto &latch = t->latch;
      delete t;
      latch.count_down();
    };
  }

  std::latch &latch;
};

int main() {
  auto now = std::chrono::steady_clock::now();
  auto exe = MtExecutor(8);
#define N (32 * 1024)
  std::latch latch2(N);
  for (int i = 0; i < N; i++) {
    exe.enqueue(new Taskkkk(latch2));
  }
  latch2.wait();
  auto end = std::chrono::steady_clock::now();
  printf("time: %f\n", std::chrono::duration<double>(end - now).count());
  return 0;
} */
// #include "runtime.hpp"

// auto taskA() -> Task<int>
// {
//   ::puts("--task A");
//   co_return 233;
// }

// auto taskB() -> Task<double>
// {
//   co_await taskA();
//   ::puts("--task B");
//   co_return 1.233;
// }

// auto main2() -> int
// {
//   auto runtime = Runtime(3);

//   auto k = runtime.block([]() -> Task<int> {
//     int a = co_await taskA();
//     double b = co_await taskB();
//     printf("%d %f\n", a, b);
//     puts("--hello world");
//     co_return 233;
//   });
//   printf("%d\n", k);
// }

// #include "timer.hpp"

// auto main() -> int
// {

// }