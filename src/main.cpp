#include "util/logger.h"
#include "video/capture.h"
//#include "web/service.h"
#include <opencv2/core/utility.hpp>
#include <signal.h>
#include <thread>

// hcam::web *web;
hcam::capture *cap;

void signal_handler(int signum) {
  hcam::logger::info("signal ", signum, " received, quitting...");
  // delete web;
  delete cap;
  exit(signum);
}

int main(int argc, char **argv) {
  // FIXME 可配置　
  cv::setNumThreads(4);
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  /*web = new hcam::web("config.json");
  web->run();*/

  cap = new hcam::capture();
  cap->run();

  /*getchar();
  raise(SIGINT);*/
  while (true)
    std::this_thread::sleep_for(std::chrono::hours(8));
}