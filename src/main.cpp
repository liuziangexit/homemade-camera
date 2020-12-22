#include "capture.h"
#include "logger.h"
#include <signal.h>
#include <web_service.h>

homemadecam::capture *capture;

void signal_handler(int signum) {
  homemadecam::logger::info("signal ", signum, " received, quitting...");
  if (capture) {
    // capture->~capture();
    delete capture;
  }
  exit(signum);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  cv::setNumThreads(0);
  // homemadecam::web web("config.json");
  // web.run();
  capture = new homemadecam::capture(homemadecam::config("config.json"));
  capture->start();
  getchar();
  return 0;
}