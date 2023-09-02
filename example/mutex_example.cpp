#include <coco/runtime.hpp>
#include <coco/sync.hpp>

static coco::Runtime rt(coco::MT, 4);

std::size_t counter = 0;

auto incTask(coco::sync::Mutex& mt) -> coco::Task<>
{
  for (int i = 0; i < 1'000'00; i++) {
    co_await mt.lock();
    counter += 1;
    mt.unlock();
  }
  co_return;
}

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
    auto mt = coco::sync::Mutex();
    auto ths = std::vector<coco::JoinHandle<coco::Task<>>>(4);
    for (int i = 0; i < ths.size(); i++) {
      ths[i] = rt.spawn(incTask(mt));
    }
    co_await rt.waitAll(ths);
    ::printf("counter = %zu\n", counter);
    co_return;
  }());
}