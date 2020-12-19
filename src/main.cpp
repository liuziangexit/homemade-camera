#include "capture.h"
#include <stdio.h>
#include <string> // for strings
#include <thread>

int main(int argc, char **argv) {
  static std::atomic<uint32_t> flag(0);
  homemadecam::capture::begin("./video/", homemadecam::capture::codec::H264,
                              3 * 60);
  getchar();
  // std::this_thread::sleep_for(std::chrono::seconds(15));
  homemadecam::capture::end();
  return 0;
}