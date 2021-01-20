#include "util/thread_pool.h"
#include <assert.h>
#include <atomic>
#include <stdexcept>
#include <stdlib.h>

namespace hcam {

thread_pool::thread_pool() : state(STOPPED) {}

void thread_pool::run(uint32_t thread_cnt) {
  {
    int expect = STOPPED;
    if (!state.compare_exchange_strong(expect, RUNNING))
      throw std::logic_error("invalid state");
  }
  while (!jobs.empty())
    jobs.pop();
  for (uint32_t i = 0; i < thread_cnt; i++)
    workers.emplace_back(&thread_pool::worker_func, this);
}

void thread_pool::stop() {
  {
    int expect = RUNNING;
    if (!state.compare_exchange_strong(expect, STOPPING))
      throw std::logic_error("invalid state");
  }

  // wake workers
  cv.notify_all();

  // wait workers to quit
  for (auto &t : workers)
    if (t.joinable())
      t.join();
  workers.clear();

  auto prev = state.exchange(STOPPED);
  assert(prev == STOPPING);
}

void thread_pool::worker_func() {
  while (state == RUNNING) {
    std::unique_lock g(jobs_m);

    if (jobs.empty()) {
      // wait for a job
      while (true) {
        cv.wait(g);
        if (state == STOPPING)
          return;
        if (!jobs.empty())
          break;
      }
    }

    // get a job
    job_t job = std::move(jobs.front());
    jobs.pop();
    jobs_m.unlock();
    // do job
    job();
  }
}

} // namespace hcam
