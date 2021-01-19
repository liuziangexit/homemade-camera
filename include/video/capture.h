#ifndef __HCAM_CAPTURE_H__
#define __HCAM_CAPTURE_H__

#include "codec.h"
#include "config/config.h"
#include <atomic>
#include <opencv2/core.hpp>
#include <opencv2/freetype.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <optional>
#include <string>
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
  // 0-未开始,1-开始,2-要求结束,3-正在结束
  std::atomic<uint32_t> m_flag = 0;
  config m_config;

  void task(config);

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
