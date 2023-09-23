#include <coco/runtime.hpp>
#include <coco/sync.hpp>
#include <random>

// TODO: not efficient in this case
static auto rt = coco::Runtime(coco::MT, 16);
constexpr auto kTaskCount = 100'000'000;

static auto gRandomGen = std::mt19937(std::random_device{}());
static auto gIterRange = std::uniform_int_distribution(0, 100);

auto heavy_task(coco::sync::Latch& latch) -> coco::Task<>
{
  auto iter = gIterRange(gRandomGen);
  for (int i = 0; i < iter; i++) {
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