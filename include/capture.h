#ifndef __HOMEMADECAM_CAPTURE_H__
#define __HOMEMADECAM_CAPTURE_H__
#include "guard.h"
#include "logger.h"
#include "time_util.h"
#include <atomic>
#include <future>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/time.h>
#include <thread>
#include <time.h>
#include <utility>

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

    std::thread(&capture::task, save_directory, c, duration).detach();
  }

  static volatile int result;

  static int end() {
    if (flag != 3) {
      flag = 2;
      while (flag != 3)
        ;
    }
    int r = (int)result;
    flag = 0;
    return r;
  }

private:
  // 0-未开始,1-开始,2-要求结束,3-正在结束
  static std::atomic<uint32_t> flag;

  static void task(const std::string &save_directory, codec c,
                   uint32_t duration) {
    guard reset_flag([]() {
      auto prev_state = flag.exchange(3);
      if (prev_state == 0 || prev_state == 3)
        throw std::runtime_error("capture flag is been tamper");
    });

    auto filename = make_filename(save_directory, file_format(c));

    result = do_capture(filename, c, duration);
  }

  static std::string make_filename(const std::string &save_directory,
                                   const std::string file_format) {
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
    std::ostringstream fmt(*dir, std::ios_base::app);
    fmt << localt->tm_year + 1900 << '-' << localt->tm_mon + 1 << '-'
        << localt->tm_mday << ' ' << localt->tm_hour << ':' << localt->tm_min
        << ':' << localt->tm_sec;
    fmt << '.' << file_format;
    return fmt.str();
  }

  static std::string file_format(codec c) {
    if (c == H264)
      return "mov";
    if (c == H265)
      return "mov";
    throw std::invalid_argument("");
  }

  static int fourcc(codec c) {
    if (c == H264)
      return cv::VideoWriter::fourcc('a', 'v', 'c', '1');
    if (c == H265)
      return cv::VideoWriter::fourcc('h', 'v', 'c', '1');
    throw std::invalid_argument("");
  }

  static int do_capture(std::string filename, codec c, uint32_t duration) {
    cv::VideoCapture capture;
    if (!capture.open(0, cv::CAP_ANY)) {
      logger::error("VideoCapture open failed");
      return 1;
    }
    double fps = (int)capture.get(cv::CAP_PROP_FPS);
    cv::Size res(capture.get(cv::CAP_PROP_FRAME_WIDTH),
                 capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    cv::VideoWriter writer;
    if (!writer.open(filename, fourcc(c), fps, res, true)) {
      logger::error("VideoWriter open failed");
      return 2;
    }

    const uint64_t frame_time = 1000 / fps;
    while (true) {
      if (flag == 2) {
        break;
      }

      auto tbegin = checkpoint(3);
      std::atomic_thread_fence(std::memory_order_seq_cst);

      cv::Mat frame;
      if (!capture.read(frame)) {
        logger::error("VideoCapture read failed");
        return 3;
      }
      writer.write(frame);

      std::atomic_thread_fence(std::memory_order_seq_cst);
      auto tend = checkpoint(3);
      if (tend - tbegin > frame_time) {
        logger::warn("low frame rate");
      } else {
        logger::info("cost ", tend - tbegin, "ms\n");
        // std::this_thread::sleep_for(std::chrono::milliseconds());
      }
    }
    capture.~VideoCapture();
    writer.~VideoWriter();
    return 0;
  }
};

std::atomic<uint32_t> capture::flag(0);
volatile int capture::result = 0;

} // namespace homemadecam

#endif