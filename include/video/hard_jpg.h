#ifndef __HCAM_HARD_JPG_H__
#define __HCAM_HARD_JPG_H__

#include <opencv2/core/mat.hpp>
#include <utility>

#ifdef __cplusplus
extern "C" {
#endif

#include "video/jpeg.h"

#ifdef __cplusplus
}
#endif

namespace hcam {

class hard_jpg {
  OPENMAX_JPEG_DECODER *pDecoder;

public:
  hard_jpg();
  ~hard_jpg();
  std::pair<bool, cv::Mat> decode(unsigned char *src, uint32_t len);
  /* std::pair<bool, void *> encode(const cv::Mat &);*/
};

} // namespace hcam

#endif