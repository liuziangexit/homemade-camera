#ifndef __HCAM_LOGGER_H__
#define __HCAM_LOGGER_H__
#include "config/config_manager.h"
#include <iomanip>
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
  static void logger_impl(uint32_t level, const std::string &module,
                          ARGS &&...args) {
    // TODO 预分配空间
    if (level < config_manager::get().log_level) {
      return;
    }
    const auto &set = config_manager::get().disable_log_module;
    if (set.find(module) != set.end()) {
      return;
    }

    std::ostringstream fmt;
    if (level == 0) {
      fmt << "[debg] ";
    } else if (level == 1) {
      fmt << "[info] ";
    } else if (level == 2) {
      fmt << "[warn] ";
    } else if (level == 3) {
      fmt << "[erro] ";
    } else if (level == 4) {
      fmt << "[fatl] ";
    } else {
      throw std::invalid_argument("");
    }
    fmt << module << " at ";

    time_t tm;
    time(&tm);
    auto localt = localtime(&tm);
    fmt << localt->tm_year + 1900 << '/' << std::setfill('0') << std::setw(2)
        << localt->tm_mon + 1 << '/' << std::setfill('0') << std::setw(2)
        << localt->tm_mday << ' ' << std::setfill('0') << std::setw(2)
        << localt->tm_hour << ':' << std::setfill('0') << std::setw(2)
        << localt->tm_min << ':' << std::setfill('0') << std::setw(2)
        << localt->tm_sec;

    fmt << ": ";
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

  template <bool f = false> static void logger_impl(std::ostringstream &fmt) {
    static_assert(f, "bad argument");
  }

  template <typename T>
  static void logger_impl(std::ostringstream &fmt, T &&cur) {
    fmt << cur;
  }

  static void logger_impl() {}

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