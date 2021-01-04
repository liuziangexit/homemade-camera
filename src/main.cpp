#include "capture.h"
#include "logger.h"
#include <signal.h>
#include <web_service.h>

homemadecam::web *web;
homemadecam::capture *cap;

void signal_handler(int signum) {
  homemadecam::logger::info("signal ", signum, " received, quitting...");
  delete web;
  delete cap;
  exit(signum);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  cv::setNumThreads(0);
  web = new homemadecam::web("config.json");
  web->run();

  homemadecam::config c("config.json");
  cap = new homemadecam::capture(c);
  cap->run();

  getchar();
  raise(SIGINT);
}
