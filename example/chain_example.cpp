#include <coco/runtime.hpp>
#include <coco/sync/chain.hpp>

static auto rt = coco::Runtime(coco::MT, 4);

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
    auto chain = coco::sync::Chain();
    std::uint64_t cnt{0};
    chain.withCtx(&cnt)
        .chain([](void* ctx) -> coco::Task<> {
          ::puts("job 1");
          auto cnt = static_cast<std::uint64_t*>(ctx);
          for (int i = 0; i < 100'000; i++) {
            *cnt += 1;
          }
          co_return;
        })
        .chain([](void* ctx) -> coco::Task<> {
          ::puts("job 2");
          auto cnt = static_cast<std::uint64_t*>(ctx);
          for (int i = 0; i < 100'000; i++) {
            *cnt += 1;
          }
          co_return;
        })
        .chain([](void* ctx) -> coco::Task<> {
          ::puts("job 3");
          auto cnt = static_cast<std::uint64_t*>(ctx);
          for (int i = 0; i < 100'000; i++) {
            *cnt += 1;
          }
          co_return;
        });
    co_await rt.wait(std::move(chain));
    assert(cnt == 300'000);
  }());
}
