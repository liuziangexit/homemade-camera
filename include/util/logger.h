#ifndef __HCAM_LOGGER_H__
#define __HCAM_LOGGER_H__
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <string>
#include <time.h>
#include <utility>

namespace hcam {
class logger {
  static std::mutex mut;

  template <typename... ARGS>
  static void logger_impl(uint32_t level, ARGS &&...args) {
    // TODO 预分配空间
    std::ostringstream fmt;
    if (level == 1) {
      fmt << "[info] ";
    } else if (level == 2) {
      fmt << "[warn] ";
    } else if (level == 3) {
      fmt << "[error] ";
    } else if (level == 4) {
      fmt << "[fatal] ";
    } else {
      throw std::invalid_argument("");
    }
    time_t tm;
    time(&tm);
    auto localt = localtime(&tm);
    fmt << localt->tm_year + 1900 << '/' << localt->tm_mon + 1 << '/'
        << localt->tm_mday << ' ' << localt->tm_hour << ':' << localt->tm_min
        << ':' << localt->tm_sec << ' ';

    logger_impl(fmt, std::forward<ARGS>(args)...);

    // TODO: 可选文件或console
    std::unique_lock<std::mutex> guard(mut);
    std::cout << fmt.str() << "\r\n";
    std::cout.flush();
  }

  template <typename T, typename... ARGS>
  static void logger_impl(std::ostringstream &fmt, T &&cur, ARGS &&...rest) {
    fmt << cur;
    logger_impl(fmt, std::forward<ARGS>(rest)...);
  }

  template <typename T>
  static void logger_impl(std::ostringstream &fmt, T &&cur) {
    fmt << cur;
  }

  static void logger_impl() {}

public:
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
std::mutex logger::mut;
} // namespace hcam

#endif