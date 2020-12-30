//#include "capture.h"
#include "ffmpeg_capture.h"
#include "ffmpeg_encoder.h"
#include "logger.h"
#include <fstream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <web_service.h>

homemadecam::web *web;

void signal_handler(int signum) {
  homemadecam::logger::info("signal ", signum, " received, quitting...");
  delete web;
  exit(signum);
}

int main(int argc, char **argv) {

  homemadecam::ffmpeg_capture cap;
  int ret = cap.open("avfoundation", "0", cv::Size{1280, 720}, 30);
  auto frame = cap.read();
  // cv::imshow("Display window", frame);

  std::fstream file("test.mov", std::ios_base::out);
  homemadecam::ffmpeg_encoder<std::fstream> enc;

  ret = enc.open(homemadecam::H264, cap.codec_context, file);

  cv::waitKey(0); // Wait for a keystroke in the window
  /*
  signal(SIGINT, signal_handler);
  cv::setNumThreads(0);
  web = new homemadecam::web("config.json");
  web->run();
  getchar();
  raise(SIGINT);*/
}
