#ifndef __HOMEMADECAM_CODEC_H__
#define __HOMEMADECAM_CODEC_H__
#include <algorithm>
#include <cctype>
#include <opencv2/videoio.hpp>
#include <stdexcept>
#include <string>

namespace homemadecam {

enum codec { H264, H265 };

static std::string codec_file_format(codec c) {
  if (c == H264)
    return "mov";
  if (c == H265)
    return "mov";
  throw std::invalid_argument("");
}

static int codec_fourcc(codec c) {
  if (c == H264)
    return cv::VideoWriter::fourcc('a', 'v', 'c', '1');
  if (c == H265)
    return cv::VideoWriter::fourcc('h', 'v', 'c', '1');
  throw std::invalid_argument("");
}

static std::string codec_to_string(codec c) {
  if (H264 == c)
    return "H264";
  if (H265 == c)
    return "H265";
  throw std::invalid_argument("");
}

static codec codec_parse(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    // TODO verify it is ASCII
    return std::tolower(c);
  });
  if (s == "h264")
    return H264;
  if (s == "h265")
    return H265;
  throw std::invalid_argument("");
}

} // namespace homemadecam

#endif