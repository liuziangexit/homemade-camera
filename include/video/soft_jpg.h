#ifndef __HCAM_SOFT_JPG_H__
#define __HCAM_SOFT_JPG_H__

#include <opencv2/core/mat.hpp>
#include <utility>

#ifdef __cplusplus
extern "C" {
#endif

#include <turbojpeg.h>

#ifdef __cplusplus
}
#endif

namespace hcam {

class soft_jpg {
  tjhandle _jpegDecompressor;

public:
  soft_jpg();
  ~soft_jpg();
  std::pair<bool, cv::Mat> decode(unsigned char *src, uint32_t len);
  /* std::pair<bool, void *> encode(const cv::Mat &);*/
};

} // namespace hcam

#endif