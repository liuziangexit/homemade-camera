//#include "capture.h"
#include "ffmpeg_capture.h"
#include "logger.h"
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <signal.h>
#include <web_service.h>

homemadecam::web *web;

void signal_handler(int signum) {
  homemadecam::logger::info("signal ", signum, " received, quitting...");
  delete web;
  exit(signum);
}

int main(int argc, char **argv) {

  homemadecam::ffmpeg_capture cap;
  int ret = cap.open_device("avfoundation", "0", cv::Size{1280, 720}, 30);
  auto frame = cap.grab();
  cv::imshow("Display window", frame);
  cv::waitKey(0); // Wait for a keystroke in the window
  cap.close_device();
  /*
  signal(SIGINT, signal_handler);
  cv::setNumThreads(0);
  web = new homemadecam::web("config.json");
  web->run();
  getchar();
  raise(SIGINT);*/
}
