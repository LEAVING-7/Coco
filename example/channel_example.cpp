#include <coco/runtime.hpp>
#include <coco/sync/channel.hpp>

static coco::Runtime rt(coco::MT, 4);

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
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
    co_return;
  }());
}