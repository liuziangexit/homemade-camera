#include "video/soft_jpg.h"
#include "util/logger.h"
#include <memory>

namespace hcam {

soft_jpg::soft_jpg() { _jpegDecompressor = tjInitDecompress(); }
soft_jpg::~soft_jpg() { tjDestroy(_jpegDecompressor); }
std::pair<bool, cv::Mat> soft_jpg::decode(unsigned char *src, uint32_t len) {
  int jpegSubsamp, width, height;
  if (tjDecompressHeader2(_jpegDecompressor, src, len, &width, &height,
                          &jpegSubsamp)) {
    if (tjGetErrorCode(_jpegDecompressor) != TJERR_WARNING) {
      logger::error("tjDecompressHeader2 failed: ", tjGetErrorStr());
      return std::pair<bool, cv::Mat>(false, cv::Mat());
    } else {
      logger::warn("tjDecompressHeader2 warning: ", tjGetErrorStr());
    }
  }
  cv::Mat mat(height, width, CV_8UC3);
  if (tjDecompress2(_jpegDecompressor, src, len, mat.data, width, 0 /*pitch*/,
                    height, TJPF_BGR, TJFLAG_FASTDCT)) {
    if (tjGetErrorCode(_jpegDecompressor) != TJERR_WARNING) {
      logger::error("tjDecompress2 failed: ", tjGetErrorStr());
      return std::pair<bool, cv::Mat>(false, cv::Mat());
    } else {
      logger::warn("tjDecompress2 warning: ", tjGetErrorStr());
    }
  }
  return std::pair<bool, cv::Mat>(true, mat);
}

} // namespace hcam
