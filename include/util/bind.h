#ifndef __HCAM_BIND_H__
#define __HCAM_BIND_H__
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

//把一个函数和给定的参数绑定成void()的样子，并且提供一个std::future
template <typename F, typename... ARGS,
          typename R = std::invoke_result_t<F, ARGS...>>
std::pair<std::function<void()>, std::future<R>> pack_task(F &&f,
                                                           ARGS &&...args) {
  auto shared_f = std::make_shared<F>(std::forward<F>(f));
  auto shared_promise = std::make_shared<
      std::promise<typename std::result_of<F(ARGS...)>::type>>();
  std::function<void()> task = std::bind(
      [shared_f, shared_promise](ARGS... args) {
        //考虑到类型R可能没有默认构造函数，或者是void，所以这样做
        using align_t = std::conditional_t<std::is_void_v<R>, unsigned char, R>;
        alignas(align_t) unsigned char //
            _result[sizeof(
                std::conditional_t<std::is_void_v<R>, unsigned char, R>)];
        R *result = nullptr;

        bool thrown = false;
        try {
          // call the function and save its return value to result
          if constexpr (std::is_void_v<R>) {
            (*shared_f)(std::forward<ARGS>(args)...);
          } else {
            result =
                ::new (&_result) R((*shared_f)(std::forward<ARGS>(args)...));
          }
        } catch (const std::exception &) {
          shared_promise->set_exception(std::current_exception());
          thrown = true;
        } catch (...) {
          shared_promise->set_exception(std::make_exception_ptr(
              std::runtime_error("exception is not std::exception!")));
          thrown = true;
        }
        if (!thrown) {
          if constexpr (std::is_void_v<R>) {
            shared_promise->set_value();
          } else {
            shared_promise->set_value(*result);
            result->~R();
          }
        }
      },
      std::forward<ARGS>(args)...);

  return std::pair<std::function<void()>, std::future<R>>(
      std::move(task), shared_promise->get_future());
}

} // namespace hcam

#endif