#include "capture.h"
#include "codec.h"
#include "config.h"
#include <stdio.h>
#include <string> // for strings
#include <thread>

int main(int argc, char **argv) {
  homemadecam::config config("config.json");
  homemadecam::capture::begin("./video/", homemadecam::codec::H264, 3 * 60);
  getchar();
  homemadecam::capture::end();
  return 0;
}