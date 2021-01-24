#include "video/soft_jpg.h"
#include <memory>
#define CHANNEL_CNT 3

namespace hcam {

soft_jpg::soft_jpg() { _jpegDecompressor = tjInitDecompress(); }
soft_jpg::~soft_jpg() { tjDestroy(_jpegDecompressor); }
std::pair<bool, cv::Mat> soft_jpg::decode(unsigned char *src, uint32_t len) {
  int jpegSubsamp, width, height;
  if (tjDecompressHeader2(_jpegDecompressor, src, len, &width, &height,
                          &jpegSubsamp))
    return std::pair<bool, cv::Mat>(false, cv::Mat());
  std::unique_ptr<unsigned char[]> output(
      new unsigned char[width * height * CHANNEL_CNT]);
  if (tjDecompress2(_jpegDecompressor, src, len, output.get(), width,
                    0 /*pitch*/, height, TJPF_BGR, TJFLAG_FASTDCT))
    return std::pair<bool, cv::Mat>(false, cv::Mat());
  return std::pair<bool, cv::Mat>(
      true, cv::Mat(height, width, CV_8UC3, output.get()));
}

} // namespace hcam
