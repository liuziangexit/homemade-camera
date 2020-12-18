#include "../include/capture.h"
#include <atomic>
#include <ctime>
#include <opencv2/core.hpp>    // Basic OpenCV structures (cv::Mat)
#include <opencv2/videoio.hpp> // Video write

namespace homemadecam {

std::atomic<bool> capturing = false;
void capture_begin(const std::string &save_directory, codec codec,
                   uint32_t duration) {
  time_t tm;
  struct tm *info;
  char buffer[80];

  time(&tm);

  info = localtime(&rawtime);
  printf("当前的本地时间和日期：%s", asctime(info));

  cv::VideoCapture capture;
}
void capture_end() {}

} // namespace homemadecam
