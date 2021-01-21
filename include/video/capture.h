#ifndef __HCAM_CAPTURE_H__
#define __HCAM_CAPTURE_H__

#include "codec.h"
#include "config/config.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <opencv2/core.hpp>
#include <opencv2/freetype.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// FIXME 结束等待时候不要spin

namespace hcam {

class capture {
public:
  capture(const config &);

  void run();

  volatile int result = 0;

  int stop();

  ~capture();

private:
  enum { STOPPED, RUNNING, STOPPING };
  std::atomic<int> state = STOPPED;
  config _config;
  // capture线程解码出来的帧，write线程会去这里拿
  std::vector<cv::Mat> frames;
  std::mutex frames_mtx;
  std::condition_variable frames_cv;
  std::thread capture_thread, write_thread;

  std::string make_filename(std::string, const std::string &);

  static bool set_input_pixelformat(cv::VideoCapture &cap, codec c) {
    cap.set(cv::CAP_PROP_FOURCC, codec_fourcc(c));
    return codec_fourcc(c) == cap.get(cv::CAP_PROP_FOURCC);
  }

  int do_capture(const config &);

  void render_text(int, const std::string &, int, std::optional<cv::Scalar>,
                   cv::freetype::FreeType2 *, cv::Mat &);
};

} // namespace hcam

#endif
