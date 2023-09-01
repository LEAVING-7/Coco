#include "coco/mt_executor.hpp"
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

#include "coco/runtime.hpp"
#include "coco/net/net.hpp"

using namespace coco;
auto runtime = Runtime(1);

auto taskA() -> Task<int>
{
  ::putchar('.');
  co_return 233;
}
auto taskC() -> Task<>
{
  ::puts("begin Task C");
  co_await runtime.sleep(2s);
  ::puts("end Task C");
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
  // 4.761779s for one thread
  co_return 1.233;
}

auto taskD() -> Task<double>
{
  ::puts("Task D");
  co_return 1.233;
}

auto main0() -> int
{
  auto k = runtime.block([&]() -> Task<int> {
    co_await runtime.waitAll(taskA(), taskD(), taskC());
    co_return 233;
  });
  printf("%d\n", k);
}

auto main() -> int {
  auto k = runtime.block([&]() -> Task<int> {
    auto addr = net::SocketAddr(net::SocketAddrV4::localhost(2333));
    auto [listener, err] = net::TcpListener::bind(addr);
    if (err != std::errc{}) {
      co_return 0;
    }
    auto [stream, err2] = co_await listener.accept();
    if (err2 != std::errc{}) {
      co_return 0;
    }
    auto [n, err3] = co_await stream.send(std::as_byte(std::span("hello world", 11)));
    if (err3 != std::errc{}) {
      co_return 0;
    }
    co_return 233;
  });
}