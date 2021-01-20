#include "util/logger.h"
#include "video/capture.h"
//#include "web/service.h"
#include "omx/omx_lib.h"
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
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  /*web = new hcam::web("config.json");
  web->run();*/

  hcam::omx_lib omxlib;

  hcam::config c("config.json");
  cap = new hcam::capture(c);
  cap->run();

  std::this_thread::sleep_for(std::chrono::hours(8));
  /* getchar();*/
  raise(SIGINT);
}