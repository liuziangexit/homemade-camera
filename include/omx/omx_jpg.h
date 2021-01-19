#ifndef __HCAM_OMX_JPG_H__
#define __HCAM_OMX_JPG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ilclient.h>

#ifdef __cplusplus
}
#endif

#include "image_def.h"
#include "util/logger.h"
#include <opencv2/core/mat.hpp>
#include <stddef.h>

namespace hcam {

class omx_jpg {
  ILCLIENT_T *client = 0;

  struct JPEG_INFO {
    int nColorComponents; // number of color components
    int mode;             // progressive or non progressive
    char orientation; // orientation according to exif tag (1..8, default: 1)
  };

  int decodeImage(const unsigned char *, uint32_t, IMAGE *);

public:
  omx_jpg();
  ~omx_jpg();
  std::pair<bool, cv::Mat> decode(unsigned char *, uint32_t);
  std::pair<bool, void *> encode(const cv::Mat &);
};

} // namespace hcam

#endif