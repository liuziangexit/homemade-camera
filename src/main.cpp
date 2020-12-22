#include "logger.h"
#include <signal.h>
#include <web_service.h>

void signal_handler(int signum) {
  homemadecam::logger::info("signal ", signum, " received, quitting...");
  exit(signum);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  homemadecam::web web("config.json");
  web.run();
  return 0;
}