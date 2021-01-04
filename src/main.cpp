//#include "capture.h"
#include "ffmpeg_capture.h"
#include "ffmpeg_encoder.h"
#include "logger.h"
#include <fstream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
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

  homemadecam::ffmpeg_capture cap;
  int ret = cap.open("avfoundation", "0", cv::Size{1280, 720}, 30);
  // cv::imshow("Display window", frame);

  std::ofstream file("test.mov", std::ios_base::out);
  homemadecam::ffmpeg_encoder<false> enc;

  ret = enc.open(homemadecam::H264, cap.codec_context, file);

  for (int i = 0; i < 100; i++) {
    auto frame = cap.read();
    enc.write(frame);
  }

  cap.close();
  enc.close();

  file.close();
  // cv::waitKey(0); // Wait for a keystroke in the window
  /*
  signal(SIGINT, signal_handler);
  cv::setNumThreads(0);
  web = new homemadecam::web("config.json");
  web->run();

  homemadecam::config c("config.json");
  cap = new homemadecam::capture(c);
  cap->run();

  getchar();
  raise(SIGINT);*/
}
