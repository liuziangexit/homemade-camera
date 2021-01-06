#ifndef __HOMEMADECAM_OMX_UTIL_H__
#define __HOMEMADECAM_OMX_UTIL_H__
#include <cstddef>
#include <opencv2/core/mat.hpp>
#include <utility>

namespace homemadecam {

class omx_util {
public:
  omx_util() {}
  std::pair<bool, cv::Mat> jpg_decode(unsigned char *src, std::size_t len) {}
  std::pair<bool, void *> jpg_encode(const cv::Mat &src) {}
};

} // namespace homemadecam

#endif