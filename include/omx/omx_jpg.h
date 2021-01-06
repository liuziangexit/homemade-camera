#ifndef __HOMEMADECAM_OMX_UTIL_H__
#define __HOMEMADECAM_OMX_UTIL_H__
#include "jpeg.h"
#include <assert.h>
#include <cstddef>
#include <opencv2/core/mat.hpp>
#include <utility>

namespace homemadecam {

class omx_jpg {
  OPENMAX_JPEG_DECODER *decoder;

public:
  omx_jpg() {
    bcm_host_init();
    assert(setupOpenMaxJpegDecoder(&pDecoder) == 0);
  }
  ~omx_jpg() { cleanup(decoder); }
  // std::pair<bool, cv::Mat>
  bool jpg_decode(unsigned char *src, std::size_t len) {
    if (decodeImage(decoder, src, len))
      return false;
    return true;
  }
  std::pair<bool, void *> jpg_encode(const cv::Mat &src) {}
};

} // namespace homemadecam

#endif