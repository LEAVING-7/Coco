#include <coco/runtime.hpp>
#include <coco/sync.hpp>

// TODO: not efficient in this case
static auto rt = coco::Runtime(coco::MT, 16);
constexpr auto kTaskCount = 100'000'000;

auto heavy_task(coco::sync::Latch& latch) -> coco::Task<>
{
  for (int i = 0; i < 100; i++) {
    // do some calculation
    std::this_thread::yield();
  }
  latch.countDown();
  co_return;
}

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
    auto latch = coco::sync::Latch(kTaskCount);
    for (int i = 0; i < kTaskCount; i++) {
      rt.spawnDetach(heavy_task(latch));
    }
    co_await latch.wait();
    co_return;
  }());
}