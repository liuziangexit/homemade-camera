#include "capture.h"
#include <stdio.h>
#include <string> // for strings
#include <thread>

int main(int argc, char **argv) {
  homemadecam::capture_begin("./video/", homemadecam::codec::H264, 3 * 60);
  getchar();
  //std::this_thread::sleep_for(std::chrono::seconds(15));
  homemadecam::capture_end();
  return 0;
}