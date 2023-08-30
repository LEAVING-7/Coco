#pragma once
#include "preclude.hpp"

#include "mt_executor.hpp"
#include <functional>

namespace coco {
class Runtime {
public:
  Runtime(std::size_t threadNum) : mExecutor(std::make_shared<MtExecutor>(threadNum)) {}

  template <typename F, typename... Args>
    requires std::invocable<F, Args...>
  auto block(F&& fn, Args&&... args) -> decltype(auto)
  {
    using result_t = std::invoke_result_t<F, Args...>;
    static_assert(coco::IsTask<result_t>, "block function must return a task");

    auto task = std::invoke(fn, std::forward<Args>(args)...);
    auto& state = task.promise().state;
    mExecutor.get()->enqueue(task.handle());
    while (state != coco::PromiseState::Done) {
      ::printf("waiting state: %d\n", std::uint8_t(state.load()));
      state.wait(coco::PromiseState::Ready);
    }
    auto result = std::move(task.promise()).result();
    return result;
  }

private:
  std::shared_ptr<MtExecutor> mExecutor;
};
} // namespace coco