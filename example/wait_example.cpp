#include <coco/runtime.hpp>
#include <coco/sync/channel.hpp>

static coco::Runtime rt(4);

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
    using ChanType = coco::sync::Channel<int, 1024>;
    auto chan = ChanType();
    auto reader = rt.spawn([](ChanType& chan) -> coco::Task<> {
      ::puts("reader");
      std::size_t sum = 0;
      int cnt = 0;
      for (int i = 0; i < 100'000; i++) {
        auto val = co_await chan.read();
        sum += val.value();
      }
      ::printf("reader done: %zu\n", sum);
      co_return;
    }(chan));
    auto writer = rt.spawn([](ChanType& chan) -> coco::Task<> {
      ::puts("writer");
      for (int i = 0; i < 100'000; ++i) {
        co_await chan.write(i);
      }
      ::puts("writer done");
      co_return;
    }(chan));

    co_await reader.join();
    chan.close();
    co_await writer.join();
    co_return 2333;
  });
  assert(result == 2333);
}