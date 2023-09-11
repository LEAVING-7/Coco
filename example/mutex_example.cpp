#include <coco/runtime.hpp>
#include <coco/sync.hpp>

static coco::Runtime rt(coco::MT, 16);

std::size_t counter = 0;

auto incTask(coco::sync::Mutex& mt) -> coco::Task<>
{
  for (int i = 0; i < 100'000; i++) {
    co_await mt.lock();
    counter += 1;
    mt.unlock();
  }
  co_return;
}

auto tryLock() -> coco::Task<>
{
  coco::sync::Mutex mt;
  auto r = co_await mt.tryLock();
  assert(r);
  mt.unlock();

  auto t1 = rt.spawn([](coco::sync::Mutex& mt) -> coco::Task<> {
    co_await mt.lock();
    co_await rt.sleepFor(1s);
    mt.unlock();
  }(mt));
  co_await rt.sleepFor(20ms);

  r = co_await mt.tryLock();
  assert(!r);
  co_await t1.join();
};

auto main() -> int
{
  rt.block(tryLock());

  rt.block([]() -> coco::Task<> {
    auto now = std::chrono::steady_clock::now();
    auto mt = coco::sync::Mutex();
    auto ths = std::vector<coco::JoinHandle<coco::Task<>>>(100);
    for (int i = 0; i < ths.size(); i++) {
      ths[i] = rt.spawn(incTask(mt));
    }
    co_await rt.waitAll(ths);
    ::printf("counter = %zu\n", counter);
    ::printf("time = %zu\n",
             std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - now).count());
    co_return;
  }());
}