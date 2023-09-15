#include <coco/runtime.hpp>
#include <cstdio>
auto main() -> int
{
  auto rt = coco::Runtime(coco::MT, 4);
  rt.block([&]() -> coco::Task<> {
    try {
      auto i = co_await rt.blockOn([]() {
        throw std::runtime_error("throw error");
        return 1;
      });
    } catch (std::runtime_error& e) {
      std::printf("error: %s\n", e.what());
    }
    co_return;
  }());
}