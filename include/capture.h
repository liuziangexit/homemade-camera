#ifndef __HOMEMADECAM_CAPTURE_H__
#define __HOMEMADECAM_CAPTURE_H__
#include "guard.h"
#include "logger.h"
#include <future>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <time.h>

namespace homemadecam {
enum codec { H264, H265 };

int fourcc(codec c) {
  if (c == H264)
    return cv::VideoWriter::fourcc('a', 'v', 'c', '1');
  if (c == H265)
    return cv::VideoWriter::fourcc('h', 'v', 'c', '1');
  throw std::invalid_argument("");
}

// 0-未开始,1-开始,2-要求结束
std::atomic<uint32_t> flag(0);
void capture_begin(const std::string &save_directory, codec c,
                   uint32_t duration) {
  {
    uint32_t expect = 0;
    if (!flag.compare_exchange_strong(expect, 1))
      throw std::logic_error("capture already running");
  }

  // TODO
  // 这个线程里的函数还需要改，包括这个begin函数的返回值是要怎么回事，都要想想
  std::thread t([save_directory] {
    guard reset_flag([]() {
      if (flag.exchange(0) == 0)
        throw std::runtime_error("capture flag is been tamper");
    });

    // TODO 处理save_dir参数的边界条件，头尾带不带空格，最后有没有slash
    //现在假设是有slash的
    //如果引用原本就是ok，那就指向引用，如果不ok，处理一份正确的出来，然后dir指向那个拷贝
    if (save_directory[save_directory.size() - 1] == ' ' ||
        save_directory[save_directory.size() - 1] != '/')
      throw new std::invalid_argument("看下注释好吗");
    const std::string *dir = &save_directory;
    //整一个文件名
    std::string filename;
    {
      time_t tm;
      time(&tm);
      char *tmp = asctime(localtime(&tm));
      tmp[strlen(tmp) - 1] = '\0';
      filename = *dir;
      filename += tmp;
      filename += ".avi";
    }

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

    // TODO 仔细想下这个sync行不行
    const uint32_t frame_ms = 1000 / fps;
    while (true) {
      if (flag == 3) {
        break;
      }
      uint64_t tbegin = time(0);
      cv::Mat frame;
      if (!capture.read(frame)) {
        logger::error("VideoCapture read failed");
        return 3;
      }
      writer.write(frame);
      uint64_t tend = time(0);
      if (tend - tbegin > frame_ms) {
        logger::warn("low frame rate");
      } else {
        std::cout << "cost " << tend - tbegin << "ms\n";
        // std::this_thread::sleep_for(std::chrono::milliseconds());
      }
    }

    return 0;
  });
  t.detach();
}

void capture_end() {
  // TODO
  flag = 3;
}

} // namespace homemadecam

#endif