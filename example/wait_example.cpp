#include <coco/runtime.hpp>
#include <coco/sync/channel.hpp>

static coco::Runtime rt(coco::INL, 4);

auto taskA() -> coco::Task<>
{
  ::puts("taskA");
  co_return;
}

auto taskB() -> coco::Task<>
{
  co_await rt.sleep(1s);
  ::puts("taskB");
  co_return;
}

auto taskC() -> coco::Task<>
{
  co_await rt.sleep(2s);
  ::puts("taskC");
  co_return;
}

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
    co_await rt.waitAll(taskA(), taskB(), taskC());
    co_return;
  }());
}