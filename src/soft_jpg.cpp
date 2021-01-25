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
  cv::Mat mat(height, width, CV_8UC3);
  if (tjDecompress2(_jpegDecompressor, src, len, mat.data, width, 0 /*pitch*/,
                    height, TJPF_BGR, TJFLAG_FASTDCT))
    return std::pair<bool, cv::Mat>(false, cv::Mat());
  return std::pair<bool, cv::Mat>(true, mat);
}

} // namespace hcam
