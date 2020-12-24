#include "capture.h"
#include "logger.h"
#include <signal.h>
#include <web_service.h>

homemadecam::web *web;

void signal_handler(int signum) {
  homemadecam::logger::info("signal ", signum, " received, quitting...");
  delete web;
  exit(signum);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  cv::setNumThreads(0);
  web = new homemadecam::web("config.json");
  web->run();
  getchar();
  web->stop();
  delete web;
  // raise(SIGINT);
}