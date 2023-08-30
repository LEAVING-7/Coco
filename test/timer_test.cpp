#include <gtest/gtest.h>

#include "timer.hpp"
struct MyJob : coco::WorkerJob {
  MyJob(int i) : i(i), WorkerJob(&run) {}

  static auto run(coco::WorkerJob* job) noexcept -> void {}
  int i;
};

using namespace std::chrono_literals;

TEST(Timer, General)
{
  auto mgr = coco::TimerManager(10);
  std::vector<MyJob*> jobs;
  for (int i = 0; i < 21; i++) {
    jobs.push_back(new MyJob(i));
  }

  auto now = std::chrono::steady_clock::now();

  for (int i = 0; i < 21; i++) {
    mgr.addTimer(now + std::chrono::milliseconds(i * 100), jobs[i]);
  }

  std::this_thread::sleep_for(1s);
  auto okJobs = mgr._debugProcessTimers();
  ASSERT_EQ(okJobs.size(), 11); // (0 ~ 10) * 100ms
  for (int i = 11; i < jobs.size() - 1; i++) {
    mgr.deleteTimer(jobs[i]->id);
  }
  std::this_thread::sleep_for(1s);
  okJobs = mgr._debugProcessTimers();
  ASSERT_EQ(okJobs.size(), 1);
  ASSERT_EQ(okJobs[0]->id, 20);
  for (auto job : jobs) {
    delete (MyJob*)job;
  }
}

TEST(Timer, AddAndDelete)
{
  auto mgr = coco::TimerManager(10);
  auto job1 = new MyJob(100);
  auto job2 = new MyJob(200);
  auto job3 = new MyJob(300);

  auto now = std::chrono::steady_clock::now();
  mgr.addTimer(now + 1s, job1);
  mgr.addTimer(now + 2s, job2);
  mgr.addTimer(now + 3s, job3);
  mgr.deleteTimer(job2->id);
  std::this_thread::sleep_for(1s);
  auto okJobs = mgr._debugProcessTimers();
  ASSERT_EQ(okJobs.size(), 1);
  ASSERT_EQ(okJobs[0]->id, job1->id);
  std::this_thread::sleep_for(2s);
  okJobs = mgr._debugProcessTimers();
  ASSERT_EQ(okJobs.size(), 1);
  ASSERT_EQ(okJobs[0]->id, job3->id);
  delete job1;
  delete job2;
  delete job3;
}