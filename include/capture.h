#ifndef __HOMEMADECAM_CAPTURE_H__
#define __HOMEMADECAM_CAPTURE_H__

#include "codec.h"
#include "guard.h"
#include "logger.h"
#include "string_util.h"
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
  static void begin(int camera_id, const std::string &save_directory, codec c,
                    uint32_t duration) {
    {
      uint32_t expect = 0;
      if (!flag.compare_exchange_strong(expect, 1))
        throw std::logic_error("capture already running");
    }

    std::thread(&capture::task, camera_id, save_directory, c, duration)
        .detach();
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

  static void task(int camera_id, const std::string &save_directory, codec c,
                   uint32_t duration) {
    guard reset_flag([]() {
      auto prev_state = flag.exchange(3);
      if (prev_state == 0 || prev_state == 3)
        throw std::runtime_error("capture flag is been tamper");
    });

    result = do_capture(camera_id, save_directory, c, duration);
  }

  static std::string make_filename(std::string save_directory,
                                   const std::string &file_format) {
    trim(save_directory);
    if (!save_directory.empty() && *save_directory.rbegin() != '/' &&
        *save_directory.rbegin() != '\\')
      save_directory.append("/");

    //整一个文件名
    time_t tm;
    time(&tm);
    auto localt = localtime(&tm);
    std::ostringstream fmt(save_directory, std::ios_base::app);
    fmt << localt->tm_year + 1900 << '-' << localt->tm_mon + 1 << '-'
        << localt->tm_mday << " at " << localt->tm_hour << '.' << localt->tm_min
        << '.' << localt->tm_sec;
    fmt << '.' << file_format;
    return fmt.str();
  }

  static int do_capture(int camera_id, const std::string &path, codec c,
                        uint32_t duration) {
    if (duration < 1)
      return 4; //短于1秒的话文件名可能重复
    cv::VideoCapture capture;
    if (!capture.open(camera_id, cv::CAP_ANY)) {
      logger::error("VideoCapture open failed");
      return 1;
    }

    double fps = (int)capture.get(cv::CAP_PROP_FPS);
    cv::Size res(capture.get(cv::CAP_PROP_FRAME_WIDTH),
                 capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    const uint64_t frame_time = 1000 / fps;

    cv::VideoWriter writer;

  OPEN_WRITER:
    auto task_begin = checkpoint(3);
    auto filename = make_filename(path, codec_file_format(c));
    if (!writer.open(filename, codec_fourcc(c), fps, res, true)) {
      logger::error("VideoWriter open ", filename, " failed");
      return 2;
    }
    logger::info("video file change to ", filename);

    while (true) {
      if (flag == 2) {
        break;
      }

      auto frame_begin = checkpoint(3);
      cv::Mat frame;
      if (!capture.read(frame)) {
        logger::error("VideoCapture read failed");
        return 3;
      }
      auto frame_read = checkpoint(3);
      writer.write(frame);
      auto frame_encode = checkpoint(3);
      if (frame_encode - frame_begin > frame_time) {
        logger::warn("low frame rate, expect ", frame_time, "ms, actual ",
                     frame_encode - frame_begin,
                     "ms(capture:", frame_read - frame_begin,
                     "ms, encode:", frame_encode - frame_read, "ms)");
      } else {
        logger::info("cost ", frame_encode - frame_begin, "ms");
      }

      //到了预定的时间，换文件
      if (frame_encode - task_begin >= duration * 1000) {
        goto OPEN_WRITER;
      }
    }
    return 0;
  }
};

std::atomic<uint32_t> capture::flag(0);
volatile int capture::result = 0;

} // namespace homemadecam

#endif