#ifndef __HOMEMADECAM_OMX_UTIL_H__
#define __HOMEMADECAM_OMX_UTIL_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "jpeg.h"

#ifdef __cplusplus
}
#endif

#include <assert.h>
#include <cstddef>
#include <opencv2/core/core_c.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <string.h>
#include <utility>

namespace homemadecam {

class omx_jpg {
  OPENMAX_JPEG_DECODER *decoder;

public:
  omx_jpg() {
    bcm_host_init();
    assert(setupOpenMaxJpegDecoder(&decoder) == 0);
  }
  ~omx_jpg() { cleanup(decoder); }
  // std::pair<bool, cv::Mat>
  bool jpg_decode(unsigned char *src, std::size_t len) {
    if (decodeImage(decoder, (char *)(src), len))
      return false;

    void *fuck = new char[decoder->pOutputBufferHeader->nFilledLen];
    memcpy(fuck, decoder->pOutputBufferHeader->pBuffer,
           decoder->pOutputBufferHeader->nFilledLen);
    
    cv::Mat picYV12 = cv::Mat(720 * 3 / 2, 1280, CV_8UC1, fuck,
                              decoder->pOutputBufferHeader->nFilledLen);
    cv::Mat picBGR;
    cv::cvtColor(picYV12, picBGR, 99);
    // CV_YUV2BGR_YV12=99
    cv::imwrite("test.bmp", picBGR); // only for test

    /* decoder->pOutputBufferHeader->nFilledLen;
     decoder->pOutputBufferHeader->pBuffer;*/

    return true;
  }
  std::pair<bool, void *> jpg_encode(const cv::Mat &src) {}
};

} // namespace homemadecam

#endif