#include "capture.h"
#include "logger.h"
#include <signal.h>

homemadecam::capture *capture;

void signal_handler(int signum) {
  homemadecam::logger::info("signal ", signum, " received, quitting...");
  if (capture) {
    delete capture;
  }
  exit(signum);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  cv::setNumThreads(0);
  capture = new homemadecam::capture(homemadecam::config("config.json"));
  capture->run();
  return 0;
}