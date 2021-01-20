#include "util/thread_pool.h"
#include <assert.h>
#include <stdexcept>
#include <stdlib.h>

namespace hcam {

thread_pool::thread_pool() : state(STOPPED) {}

void thread_pool::run(uint32_t thread_cnt) {
  assert(state == STOPPED);
  for (uint32_t i = 0; i < thread_cnt; i++) {
    workers.emplace_back(&thread_pool::worker_func, this);
  }
  state = RUNNING;
}

void thread_pool::stop() {
  assert(state == RUNNING);
  state = STOPPING;

  // wake workers
  cv.notify_all();

  // wait workers to quit
  for (auto &t : workers)
    if (t.joinable())
      t.join();
  workers.clear();

  // clear jobs
  while (!jobs.empty())
    jobs.pop();

  state = STOPPED;
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
