#include "util/logger.h"
#include "video/capture.h"
//#include "web/service.h"
#include "omx/omx_lib.h"
#include <iostream>
#include <mcheck.h>
#include <signal.h>

// hcam::web *web;
hcam::capture *cap;

void signal_handler(int signum) {
  hcam::logger::info("signal ", signum, " received, quitting...");
  // delete web;
  delete cap;
  muntrace();
  exit(signum);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  /*web = new hcam::web("config.json");
  web->run();*/

  mtrace();

  hcam::omx_lib omxlib;

  hcam::config c("config.json");
  cap = new hcam::capture(c);
  cap->run();

  getchar();

  raise(SIGINT);
}