#ifndef __HCAM_THREAD_POOL_H__
#define __HCAM_THREAD_POOL_H__
#include "bind.h"
#include <assert.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace hcam {

class thread_pool {
  using job_t = std::function<void()>;
  std::vector<std::thread> workers;
  std::queue<job_t> jobs;
  std::mutex jobs_m;
  std::atomic<int> state;
  std::condition_variable cv;

  enum { STOPPED, RUNNING, STOPPING };

public:
  template <typename F, typename... ARGS,
            typename R = std::invoke_result_t<F, ARGS...>>
  std::future<R> async(F &&f, ARGS &&...args) {
    assert(state == RUNNING);
    auto pack = pack_task(std::forward<F>(f), std::forward<ARGS>(args)...);
    std::lock_guard g(jobs_m);
    jobs.push(std::move(pack.first));
    cv.notify_one();
    return std::future<R>(std::move(pack.second));
  }

  thread_pool();
  void run(uint32_t);
  void stop();

private:
  void worker_func();
};

} // namespace hcam

#endif