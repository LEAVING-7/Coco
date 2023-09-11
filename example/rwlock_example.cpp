#include <coco/runtime.hpp>
#include <coco/sync/rwlock.hpp>

std::vector<int> data;
static auto rt = coco::Runtime(coco::MT, 4);

auto reader(coco::sync::RwLock& lock, int i) -> coco::Task<>
{
  co_await rt.sleepFor(std::chrono::seconds(3));
  co_await lock.read();
  ::printf("data size: %zu\n", data.size());
  lock.unlockRead();
}

auto writer(coco::sync::RwLock& lock) -> coco::Task<>
{
  co_await lock.write();
  ::puts("write data");
  data.push_back(1);
  lock.unlockWrite();
}

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
    coco::sync::RwLock lock;
    constexpr int rc = 5;
    constexpr int wc = 5;
    auto readers = std::vector<coco::JoinHandle<coco::Task<>>>();
    auto writers = std::vector<coco::JoinHandle<coco::Task<>>>();

    for (int i = 0; i < rc; i++) {
      readers.push_back(rt.spawn(reader(lock, i)));
    }
    for (int i = 0; i < wc; i++) {
      writers.push_back(rt.spawn(writer(lock)));
    }

    co_await rt.waitAll(std::span(readers));
    co_await rt.waitAll(std::span(writers));
  }());
}