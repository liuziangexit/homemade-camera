#include "capture.h"
#include "config.h"
#include <stdio.h>

int main(int argc, char **argv) {
  homemadecam::config config("config.json");
  homemadecam::capture::begin(config.camera_id, config.save_location,
                              config.codec, config.duration);
  getchar();
  homemadecam::capture::end();
  return 0;
}