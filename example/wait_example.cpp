#include <coco/runtime.hpp>

static coco::Runtime rt(10);

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
  auto result = rt.block([]() -> coco::Task<int> {
    co_await rt.waitAll(taskA(), taskB(), taskC());
    ::puts("all done");
    co_return 2333;
  });
}