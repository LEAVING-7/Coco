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
    mgr.addTimer(now + std::chrono::milliseconds(i * 10), jobs[i]);
  }
  std::this_thread::sleep_for(100ms);
  auto okJobs = mgr.processTimers();
  int cnt = 0;
  while (auto job = okJobs.popFront()) {
    ASSERT_EQ(job->id, cnt++);
  }
  ASSERT_EQ(cnt, 11); // (0 ~ 10) * 10ms
  for (int i = 11; i < jobs.size() - 1; i++) {
    mgr.deleteTimer(jobs[i]->id);
  }
  std::this_thread::sleep_for(100ms);
  okJobs = mgr.processTimers();
  auto job = okJobs.popFront();
  ASSERT_EQ(job->id, 20);
  ASSERT_TRUE(okJobs.empty());
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
  mgr.addTimer(now + 100ms, job1);
  mgr.addTimer(now + 200ms, job2);
  mgr.addTimer(now + 300ms, job3);
  mgr.deleteTimer(job2->id);

  std::this_thread::sleep_for(100ms);
  auto okJobs = mgr.processTimers();
  auto job = okJobs.popFront();
  ASSERT_EQ(job->id, job1->id);
  ASSERT_EQ(okJobs.empty(), 1);
  ASSERT_EQ(mgr.nextInstant(), now + 200ms);
  
  std::this_thread::sleep_for(200ms);
  okJobs = mgr.processTimers();
  job = okJobs.popFront();
  ASSERT_EQ(job->id, job3->id);
  ASSERT_TRUE(okJobs.empty());

  std::this_thread::sleep_for(100ms);
  okJobs = mgr.processTimers();
  ASSERT_TRUE(okJobs.empty());
  ASSERT_EQ(mgr.nextInstant(), coco::Instant::max());

  delete job1;
  delete job2;
  delete job3;
}