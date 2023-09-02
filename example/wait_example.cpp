#include <coco/runtime.hpp>
#include <coco/sync/channel.hpp>

static coco::Runtime rt(coco::MT, 4);

auto taskA() -> coco::Task<>
{
  ::puts("taskA");
  co_return;
}

auto taskB() -> coco::Task<>
{
  co_await rt.sleepFor(1s);
  ::puts("taskB");
  co_return;
}

auto taskC() -> coco::Task<>
{
  co_await rt.sleepFor(2s);
  ::puts("taskC");
  co_return;
}

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
    auto [handle, id] = co_await coco::ThisTask();
    ::printf("main task %p id = %zu\n", handle.address(), id);

    co_await rt.waitAll(taskA(), taskB(), taskC());
    ::puts("everything done");
    co_return;
  }());
}