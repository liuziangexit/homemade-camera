#ifndef __HOMEMADECAM_CODEC_H__
#define __HOMEMADECAM_CODEC_H__
#include <algorithm>
#include <cctype>
#include <opencv2/videoio.hpp>
#include <stdexcept>
#include <string>

namespace homemadecam {

enum codec { RAW, H264, H265, MPEG2, MPEG4, MJPEG };

static std::string codec_file_format(codec c) {
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

static int codec_fourcc(codec c) {
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

static std::string codec_to_string(codec c) {
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

static codec codec_parse(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    // TODO verify it is ASCII
    return std::tolower(c);
  });
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

} // namespace homemadecam

#endif