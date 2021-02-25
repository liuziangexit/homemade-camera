#ifndef __HCAM_LOGGER_H__
#define __HCAM_LOGGER_H__
#include "config/config.h"
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <string>
#include <time.h>
#include <utility>

namespace hcam {
class logger {
  struct log {
    uint32_t level;
    time_t time;
    std::string message;
  };
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