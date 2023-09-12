#include <coco/runtime.hpp>
#include <coco/sync/latch.hpp>

auto main() -> int
{
  // sleep sort
  static auto rt = coco::Runtime(coco::MT, 4);
  rt.block([]() -> coco::Task<> {
    std::vector<int> vec{400, 200, 300, 100, 500};
    std::vector<int> result{};
    auto latch = coco::sync::Latch(vec.size());
    auto ths = std::vector<coco::JoinHandle<coco::Task<>>>{};
    for (auto i = 0; i < vec.size(); ++i) {
      auto task = [](coco::sync::Latch& latch, int time, int i, std::vector<int>& result) -> coco::Task<> {
        co_await rt.sleepFor(std::chrono::milliseconds(time));
        result.push_back(time);
        ::printf("Task %d done\n", i);
        latch.countDown();
      };
      ths.push_back(rt.spawn(task(latch, vec[i], i, result)));
    }
    co_await latch.wait();
    ::printf("All tasks done, result: ");
    for (auto i : result) {
      ::printf("%d ", i);
    }
    ::putchar('\n');
    co_await rt.waitAll(std::span(ths));
  }());
}