#include "omx/omx_jpg.h"
#include "omx/omx_lib.h"
#include "util/logger.h"
#include "video/capture.h"
#include "video/v4l_capture.h"
#include <signal.h>
#include <stdio.h>
//#include <web_service.h>

// homemadecam::web *web;
homemadecam::capture *cap;

void signal_handler(int signum) {
  homemadecam::logger::info("signal ", signum, " received, quitting...");
  //  delete web;
  delete cap;
  exit(signum);
}

int main(int argc, char **argv) {
  // cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_VERBOSE);

  signal(SIGINT, signal_handler);
  //  web = new homemadecam::web("config.json");
  //  web->run();

  /*  homemadecam::config c("config.json");
    cap = new homemadecam::capture(c);
    cap->run();*/

  homemadecam::v4l_capture v4l;
  if (v4l.open("/dev/video0",
               homemadecam::v4l_capture::graphic{1280, 720, 30})) {
    homemadecam::logger::fatal("v4l open failed");
    abort();
  }

  std::shared_ptr<homemadecam::v4l_capture::buffer> jpg;
  homemadecam::logger::info("read loop begin");
  for (int i = 0; i < 2; i++) {
    auto frame = v4l.read();
    if (!frame.first) {
      homemadecam::logger::fatal("v4l read failed");
      abort();
    }
    jpg = frame.second;
  }
  homemadecam::logger::info("read loop end");

  homemadecam::omx_lib lib;
  homemadecam::omx_jpg omx;
  auto decode =
      omx.jpg_decode(static_cast<unsigned char *>(jpg->data), jpg->length);

  FILE *fp;
  fp = fopen("/web/test.jpg", "wb");
  if (!fp) {
    homemadecam::logger::fatal("fopen failed");
    abort();
  }

  if (jpg->length != fwrite(jpg->data, 1, jpg->length, fp)) {
    homemadecam::logger::fatal("fwrite failed");
    abort();
  }

  fclose(fp);

  homemadecam::logger::info("jpg ok");

  getchar();
  raise(SIGINT);
}
