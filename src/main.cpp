#include "util/logger.h"
#include "video/capture.h"
#include "web/web_service.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <opencv2/core/utility.hpp>
#include <signal.h>
#include <thread>

hcam::web_service *web;
hcam::capture *cap;

std::atomic<bool> quit = false;
std::mutex quit_mut;
std::condition_variable quit_cv;

bool is_quit() { return quit; }

void signal_handler(int signum) {
  {
    bool expect = false;
    if (!quit.compare_exchange_strong(expect, true))
      return;
  }
  hcam::logger::info("main", "signal ", signum, " received, quitting...");
  cap->stop();
  web->stop();
  delete cap;
  delete web;
  std::unique_lock l(quit_mut);
  quit_cv.notify_one();
  l.unlock();
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
  std::unique_lock l(quit_mut);
  quit_cv.wait(l, is_quit);
}