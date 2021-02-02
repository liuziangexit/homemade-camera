#ifndef __HCAM_CODEC_H__
#define __HCAM_CODEC_H__
#include <algorithm>
#include <cctype>
#ifdef __linux__
#include <linux/videodev2.h>
#endif
#include <opencv2/videoio.hpp>
#include <stdexcept>
#include <string>

namespace hcam {

enum codec { RAW, H264, H265, MPEG2, MPEG4, MJPEG, YUV420 };

static inline std::string codec_file_format(codec c) {
  // suppress unused function warning
  (void)codec_file_format;
  if (c == YUV420)
    return "yuv";
  if (c == MJPEG)
    return "mov";
  if (c == MPEG2)
    return "mov";
  if (c == MPEG4)
    return "mov";
  if (c == RAW)
    return "mov";
  if (c == H264)
    return "mov";
  if (c == H265)
    return "mov";
  throw std::invalid_argument("");
}

static inline int codec_fourcc(codec c) {
  // suppress unused function warning
  (void)codec_fourcc;
  if (c == YUV420)
    throw std::invalid_argument("");
  if (c == MJPEG)
    return cv::VideoWriter::fourcc('m', 'j', 'p', 'g');
  if (c == MPEG2)
    return cv::VideoWriter::fourcc('l', 'm', 'p', '2');
  if (c == MPEG4)
    return cv::VideoWriter::fourcc('m', 'p', 'g', '4');
  if (c == RAW)
    return cv::VideoWriter::fourcc('r', 'a', 'w', ' ');
  if (c == H264)
    return cv::VideoWriter::fourcc('a', 'v', 'c', '1');
  if (c == H265)
    return cv::VideoWriter::fourcc('h', 'v', 'c', '1');
  throw std::invalid_argument("");
}

#ifdef __linux__
static int codec_v4l2_pix_fmt(codec c) {
  // suppress unused function warning
  (void)codec_to_string;
  if (c == YUV420)
    return V4L2_PIX_FMT_YUV420;
  if (c == MJPEG)
    return V4L2_PIX_FMT_MJPEG;
  throw std::invalid_argument("");
}
#endif

static inline std::string codec_to_string(codec c) {
  // suppress unused function warning
  (void)codec_to_string;
  if (YUV420 == c)
    return "YUV";
  if (MJPEG == c)
    return "MJPG";
  if (MPEG2 == c)
    return "MPEG2";
  if (MPEG4 == c)
    return "MPEG4";
  if (RAW == c)
    return "RAW";
  if (H264 == c)
    return "H264";
  if (H265 == c)
    return "H265";
  throw std::invalid_argument("");
}

static inline codec codec_parse(std::string s) {
  // suppress unused function warning
  (void)codec_parse;
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    // TODO verify it is ASCII
    return std::tolower(c);
  });
  if (s == "yuv")
    return YUV420;
  if (s == "mjpg")
    return MJPEG;
  if (s == "mpeg2")
    return MPEG2;
  if (s == "mpeg4")
    return MPEG4;
  if (s == "raw")
    return RAW;
  if (s == "h264")
    return H264;
  if (s == "h265")
    return H265;
  throw std::invalid_argument("");
}

} // namespace hcam

#endif