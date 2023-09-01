#include <coco/runtime.hpp>
#include <coco/sync.hpp>

static coco::Runtime rt(8);

std::size_t counter = 0;

auto incTask(coco::sync::Mutex& mt) -> coco::Task<>
{
  for (int i = 0; i < 100; i++) {
    co_await mt.lock();
    counter += 1;
    mt.unlock();
  }
}

auto sleepTask(coco::sync::Mutex& mt) -> coco::Task<>
{
  ::puts("hello");
  co_await mt.lock();
  co_await rt.sleep(2s);
  mt.unlock();
}

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
    coco::sync::Mutex mt;
    for (int i = 0; i < 1000; i++) {
      rt.spawnDetach(incTask(mt));
    }
    co_await rt.sleep(5s); // give some time to finish
    ::printf("counter: %zu\n", counter);
    assert(counter == 1000 * 100);
    co_return;
  });

  rt.block([]() -> coco::Task<> {
    coco::sync::Mutex mt;
    auto th = rt.spawn(sleepTask(mt));
    co_await mt.lock();
    ::puts("resume");
    mt.unlock();
    co_await th.join();
    co_return;
  });
}