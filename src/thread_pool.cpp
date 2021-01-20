#include "util/thread_pool.h"
#include <assert.h>
#include <stdexcept>
#include <stdlib.h>

namespace hcam {

thread_pool::thread_pool() : state(STOPPED) {}

template <typename F, typename... ARGS>
std::future<typename std::result_of<F(ARGS...)>::type>
thread_pool::async(F &&f, ARGS &&...args) {
  assert(state == RUNNING);
  static_assert(std::is_function_v<F>, "F not callable");

  F *f_copy = ::new F(std::forward<F>(f));
  auto promise = ::new (std::nothrow)
      std::promise<typename std::result_of<F(ARGS...)>::type>;
  if (!promise) {
    ::delete (f_copy);
    throw std::bad_alloc();
  }
  try {
    std::function<void()> bundle = std::bind(
        [f_copy, promise](ARGS... args) {
          //如果这个返回值类型没有默认构造怎么办？。。。
          typename std::result_of<F(ARGS...)>::type result;
          bool thrown = false;
          try {
            result = (*f_copy)(std::forward<ARGS>(args)...);
          } catch (const std::exception &e) {
            promise->set_exception(e);
            thrown = true;
          }
          if (!thrown) {
            promise->set_value(result);
          }
          ::delete (f_copy);
          ::delete (promise);
        },
        std::forward<ARGS>(args)...);

    std::lock_guard g(jobs_m);
    jobs.push(std::move(bundle));
  } catch (...) {
    ::delete f_copy;
    ::delete (promise);
    throw std::current_exception();
  }

  cv.notify_one();
  return promise->get_future();
}

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
