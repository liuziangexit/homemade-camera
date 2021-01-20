#ifndef __HCAM_THREAD_POOL_H__
#define __HCAM_THREAD_POOL_H__
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
            //考虑到类型R可能没有默认构造函数，或者是void，所以这样做
            using align_t =
                std::conditional_t<std::is_void_v<R>, unsigned char, R>;
            alignas(align_t) unsigned char //
                _result[sizeof(
                    std::conditional_t<std::is_void_v<R>, unsigned char, R>)];
            R *result = nullptr;

            bool thrown = false;
            try {
              // call the function and save its return value to result
              if constexpr (std::is_void_v<R>) {
                (*f_copy)(std::forward<ARGS>(args)...);
              } else {
                result =
                    ::new (&_result) R((*f_copy)(std::forward<ARGS>(args)...));
              }
            } catch (const std::exception &) {
              promise->set_exception(std::current_exception());
              thrown = true;
            } catch (...) {
              promise->set_exception(std::make_exception_ptr(
                  std::runtime_error("exception is not std::exception!")));
              thrown = true;
            }
            if (!thrown) {
              if constexpr (std::is_void_v<R>) {
                promise->set_value();
              } else {
                promise->set_value(*result);
                result->~R();
              }
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
      std::rethrow_exception(std::current_exception());
    }

    cv.notify_one();
    return promise->get_future();
  }

  thread_pool();
  void run(uint32_t);
  void stop();

private:
  void worker_func();
};

} // namespace hcam

#endif