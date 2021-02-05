#include "util/logger.h"
#include "video/capture.h"
#include "web/web.h"
#include <opencv2/core/utility.hpp>
#include <signal.h>

hcam::capture *cap;

int quit = 0;

void signal_handler(int signum) {
  if (quit != 0) {
    return;
  }
  quit = 1;
  hcam::logger::info("main", "signal ", signum, " received, quitting...");
  cap->stop();
  delete cap;
  quit = true;
  quit = 2;
  exit(signum);
}

int main(int argc, char **argv) {
  cv::setNumThreads(hcam::config_manager::get().video_thread_count);
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  cap = new hcam::capture();
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