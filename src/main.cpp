#include "capture.h"
#include "config.h"
#include <stdio.h>

int main(int argc, char **argv) {
  homemadecam::config config("config.json");
  homemadecam::capture capture(config);
  getchar();
  capture.end();
  return 0;
}