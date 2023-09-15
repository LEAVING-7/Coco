#include <coco/runtime.hpp>
#include <cstdio>
using namespace std::literals;

auto main() -> int
{
  static auto rt = coco::Runtime(coco::INL, 4);
  rt.block([&]() -> coco::Task<> {
    auto t1 = rt.spawn([]() -> coco::Task<> {
      for (int i = 0; i < 10; i++) {
        std::printf("t1: %d\n", i);
        co_await rt.sleepFor(0.8s);
      }
    }());
    try {
      auto i = co_await rt.blockOn([]() {
        std::this_thread::sleep_for(4s); // simulate a long running task
        throw std::runtime_error("throw error");
        return 1;
      });
    } catch (std::runtime_error& e) {
      std::printf("error: %s\n", e.what());
    }
    co_await t1.join();
    co_return;
  }());
}