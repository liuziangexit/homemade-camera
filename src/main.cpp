#include "util/logger.h"
#include "video/capture.h"
#include "web/web_service.h"
#include <opencv2/core/utility.hpp>
#include <signal.h>

hcam::web_service *web;
hcam::capture *cap;

int quit = 0;

void signal_handler(int signum) {
  if (quit != 0) {
    return;
  }
  quit = 1;
  hcam::logger::info("main", "signal ", signum, " received, quitting...");
  cap->stop();
  web->stop();
  delete cap;
  delete web;
  quit = true;
  quit = 2;
  exit(signum);
}

int main(int argc, char **argv) {
  cv::setNumThreads(hcam::config_manager::get().video_thread_count);
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  web = new hcam::web_service();
  web->run();

  cap = new hcam::capture(web);
  cap->run();

  /*getchar();
  raise(SIGINT);*/
  // FIXME 卧槽，这就是UB吗？
  // std::this_thread::sleep_for(std::chrono::hours::max());
  while (quit != 2) {
    pause();
  }
  /*hcam::logger::info("main", "pause done");*/
}