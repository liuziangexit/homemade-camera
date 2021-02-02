#include "util/logger.h"
#include "video/capture.h"
#include "web/web_service.h"
#include <opencv2/core/utility.hpp>
#include <signal.h>
#include <thread>

hcam::web_service *web;
hcam::capture *cap;

void signal_handler(int signum) {
  hcam::logger::info("signal ", signum, " received, quitting...");
  delete web;
  delete cap;
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
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours::max());
  }
}