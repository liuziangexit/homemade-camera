#include "capture.h"
#include "logger.h"
#include "v4l_capture.h"
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
  if (v4l.open("/dev/video0")) {
    homemadecam::logger::fatal("v4l open failed");
    abort();
  }
  auto frame = v4l.read();
  if (!frame.first) {
    homemadecam::logger::fatal("v4l read failed");
    abort();
  }

  FILE *fp;
  fp = fopen("/web/test.jpg", "wb");
  if (!fp) {
    homemadecam::logger::fatal("fopen failed");
    abort();
  }

  if (frame.second->length !=
      fwrite(frame.second->data, 1, frame.second->length, fp)) {
    homemadecam::logger::fatal("fwrite failed");
    abort();
  }

  fclose(fp);

  getchar();
  raise(SIGINT);
}
