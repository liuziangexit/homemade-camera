#ifndef __HOMEMADECAM_LOGGER_H__
#define __HOMEMADECAM_LOGGER_H__
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <string>
#include <time.h>

namespace homemadecam {
class logger {
  static void logger_impl(uint32_t level, const std::string &content) {
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

    char *tmp = asctime(localtime(&tm));
    tmp[strlen(tmp) - 1] = '\0';
    fmt << tmp << ' ' << content << "\r\n";
    // TODO: 可选文件或console
    std::cout << fmt.str();
  }

public:
  static void info(const std::string &content) { logger_impl(1, content); }
  static void warn(const std::string &content) { logger_impl(2, content); }
  static void error(const std::string &content) { logger_impl(3, content); }
  static void fatal(const std::string &content) { logger_impl(4, content); }
};
} // namespace homemadecam

#endif