#ifndef __HCAM_LOGGER_H__
#define __HCAM_LOGGER_H__
#include "config/config.h"
#include "ipc/ipc.h"
#include <atomic>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <string>
#include <thread>
#include <time.h>
#include <type_traits>
#include <utility>

namespace hcam {

class logger {
  struct log {
    uint32_t level;
    time_t time;
    std::string message;
  };
  static std::atomic<bool> quit;
  static std::mutex mut;
  static std::condition_variable cv;
  static std::queue<log> log_queue;

  template <typename... ARGS>
  static void logger_impl(uint32_t level, ARGS &&...args) {
    time_t tm;
    time(&tm);

    std::ostringstream os;
    format(os, std::forward<ARGS>(args)...);

    std::unique_lock<std::mutex> l(mut);
    log_queue.push(log{level, tm, os.str()});
    cv.notify_one();
    std::cout << os.str() << "\r\n";
  }

  template <typename T, typename... ARGS>
  static void format(std::ostringstream &fmt, T &&cur, ARGS &&...rest) {
    fmt << cur;
    format(fmt, std::forward<ARGS>(rest)...);
  }

  template <bool f = false> static void format(std::ostringstream &fmt) {
    static_assert(f, "bad argument");
  }

  template <typename T> static void format(std::ostringstream &fmt, T &&cur) {
    fmt << cur;
  }

  static void format() {}

public:
  static void start_logger(int fd) {
    std::thread([fd] {
      while (true) {
        std::unique_lock<std::mutex> l(mut);
        cv.wait(l, [] { return !log_queue.empty() || quit; });
        if (quit) {
          return;
        }
        log message = log_queue.front();
        log_queue.pop();
        l.unlock();

        // send to logger process
        union {
          uint32_t ui32;
          time_t t;
          unsigned char plain[std::conditional_t<
              (sizeof(uint32_t) > sizeof(time_t)),
              std::integral_constant<std::size_t, sizeof(uint32_t)>,
              std::integral_constant<std::size_t, sizeof(time_t)>>::value];
        } punning{};
        punning.ui32 = message.level;
        if (!ipc::send(fd, punning.plain, sizeof(message.level)))
          goto ERROR;
        punning.t = message.time;
        if (!ipc::send(fd, punning.plain, sizeof(message.time)))
          goto ERROR;
        if (!ipc::send(fd, message.message.c_str()))
          goto ERROR;
        if (1 != ipc::wait(fd, 1000))
          goto ERROR;
        auto ack = ipc::recv(fd);
        if (!ack.first)
          goto ERROR;
        std::string ack_str((char *)ack.second.content, ack.second.size);
        if (ack_str != "ACK")
          goto ERROR;
      }
    ERROR:
      std::cout << "logger thread abort\r\n";
      abort();
    }).detach();
  }

  static inline void stop_logger() {
    quit = true;
    cv.notify_one();
  }

  template <typename... ARGS> static void debug(ARGS &&...args) {
    logger_impl(0, std::forward<ARGS>(args)...);
  }

  template <typename... ARGS> static void info(ARGS &&...args) {
    logger_impl(1, std::forward<ARGS>(args)...);
  }
  template <typename... ARGS> static void warn(ARGS &&...args) {
    logger_impl(2, std::forward<ARGS>(args)...);
  }
  template <typename... ARGS> static void error(ARGS &&...args) {
    logger_impl(3, std::forward<ARGS>(args)...);
  }
  template <typename... ARGS> static void fatal(ARGS &&...args) {
    logger_impl(4, std::forward<ARGS>(args)...);
  }
};
} // namespace hcam

#endif