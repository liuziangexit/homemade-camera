#ifndef __HCAM_THREAD_POOL_H__
#define __HCAM_THREAD_POOL_H__
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
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
  thread_pool();
  template <typename F, typename... ARGS>
  std::future<typename std::result_of<F(ARGS...)>::type> async(F &&,
                                                               ARGS &&...);
  void run(uint32_t);
  void stop();

private:
  void worker_func();
};

} // namespace hcam

#endif