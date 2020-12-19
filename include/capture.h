#ifndef __HOMEMADECAM_CAPTURE_H__
#define __HOMEMADECAM_CAPTURE_H__
#include "guard.h"
#include "logger.h"
#include <atomic>
#include <future>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <time.h>

namespace homemadecam {

class capture {
public:
  enum codec { H264, H265 };
  static void begin(const std::string &save_directory, codec c,
                    uint32_t duration) {
    {
      uint32_t expect = 0;
      if (!flag.compare_exchange_strong(expect, 1))
        throw std::logic_error("capture already running");
    }

    // TODO
    // 这个线程里的函数还需要改，包括这个begin函数的返回值是要怎么回事，都要想想
    result = std::move(std::async(std::launch::async, &capture::task,
                                  save_directory, c, duration));
  }
  static std::future<int> result;

  static int end() {
    // TODO
    flag = 3;
    return result.get();
  }

private:
  // 0-未开始,1-开始,2-要求结束
  static std::atomic<uint32_t> flag;

  static int task(const std::string &save_directory, codec c,
                  uint32_t duration) {
    guard reset_flag([]() {
      if (flag.exchange(0) == 0)
        throw std::runtime_error("capture flag is been tamper");
    });

    auto filename = make_filename(save_directory);
    std::cout << filename << "\n";

    while (true) {
      std::cout << "spin!\n";
      std::this_thread::sleep_for(std::chrono::seconds(3));
      if (flag == 3) {
        std::cout << "quit!\n";
        break;
      }
    }

    return 0;
  }

  static std::string make_filename(const std::string &save_directory) {
    // TODO 处理save_dir参数的边界条件，头尾带不带空格，最后有没有slash
    //现在假设是有slash的
    //如果引用原本就是ok，那就指向引用，如果不ok，处理一份正确的出来，然后dir指向那个拷贝
    if (save_directory[save_directory.size() - 1] == ' ' ||
        save_directory[save_directory.size() - 1] != '/')
      throw new std::invalid_argument("看下注释好吗");
    const std::string *dir = &save_directory;
    //整一个文件名
    time_t tm;
    time(&tm);
    auto localt = localtime(&tm);
    std::ostringstream fmt(*dir);
    fmt << localt->tm_year + 1900 << '/' << localt->tm_mon + 1 << '/'
        << localt->tm_mday << ' ' << localt->tm_hour << ':' << localt->tm_min
        << ':' << localt->tm_sec;
    // TODO 这个后缀看看怎么搞
    fmt << ".avi";
    return fmt.str();
  }
};

std::atomic<uint32_t> capture::flag(0);
std::future<int> capture::result;

} // namespace homemadecam

#endif