#include "capture.h"
#include "logger.h"
#include <signal.h>
#include <web_service.h>

void signal_handler(int signum) {
  homemadecam::logger::info("signal ", signum, " received, quitting...");
  exit(signum);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  cv::setNumThreads(0);
  homemadecam::web web("config.json");
  try {
    web.run();
  } catch (const std::exception &ex) {
    std::cout << ex.what();
  }
  return 0;
}