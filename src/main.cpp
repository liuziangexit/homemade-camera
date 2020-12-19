#include "capture.h"
#include <stdio.h>
#include <string> // for strings
#include <thread>

int main(int argc, char **argv) {
  homemadecam::capture::begin("./video/", homemadecam::capture::codec::H264,
                              3 * 60);
  getchar();
  homemadecam::capture::end();
  return 0;
}