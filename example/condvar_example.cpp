#include <coco/runtime.hpp>
#include <coco/sync/condvar.hpp>

auto worker(coco::sync::CondVar& cv, int v) -> coco::Task<void>
{
  ::printf("worker %d wait\n", v);
  co_await cv.wait();
  ::printf("worker %d wake up\n", v);
  co_return;
}

auto main() -> int
{
  static auto rt = coco::Runtime(coco::MT, 4);
  rt.block([]() -> coco::Task<> {
    auto cv = coco::sync::CondVar();
    auto workers = std::vector<coco::JoinHandle<coco::Task<>>>();
    workers.reserve(10);
    for (auto i = 0; i < 10; ++i) {
      workers.push_back(rt.spawn(worker(cv, i)));
    }
    co_await rt.sleepFor(std::chrono::seconds(3));
    ::printf("notify all\n");
    cv.notifyAll();
    co_await rt.waitAll(workers);
  }());
}