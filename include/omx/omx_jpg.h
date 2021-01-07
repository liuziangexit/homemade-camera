#ifndef __HOMEMADECAM_OMX_UTIL_H__
#define __HOMEMADECAM_OMX_UTIL_H__

#include "omx_image.h"
#include <assert.h>
#include <cstddef>
#include <opencv2/core/core_c.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <utility>

namespace homemadecam {

class omx_jpg {
  ILCLIENT_T *client = 0;

  struct JPEG_INFO {
    int nColorComponents; // number of color components
    int mode;             // progressive or non progressive
    char orientation; // orientation according to exif tag (1..8, default: 1)
  };

  int decodeImage(const char *src, uint32_t len, IMAGE *image) {
    FILE *imageFile = fmemopen(src, len, "rb");

    if (!imageFile) {
      return -1;
    }

    char magNum[8];
    if (fread(&magNum, 1, 8, imageFile) != 8) {
      fclose(imageFile);
      return -2;
    }
    rewind(imageFile);

    int ret;

    if (memcmp(magNum, magNumJpeg, sizeof(magNumJpeg)) == 0) {
      /*  if (soft || jInfo.mode == JPEG_MODE_PROGRESSIVE ||
            jInfo.nColorComponents != 3) {
          if (info)
            printf("Soft decode jpeg\n");
          ret = softDecodeJpeg(imageFile, image);
        } else {*/
      ret = omxDecodeJpeg(client, imageFile, image);
    } else {
      printf("Unsupported image\n");
      fclose(imageFile);
      return -3;
    }

    fclose(imageFile);

    if (info)
      printf("Width: %u, Height: %u\n", image->width, image->height);

    return ret;
  }

public:
  omx_jpg() {
    if ((client = ilclient_init()) == NULL) {
      fprintf(stderr, "Error init ilclient\n");
      throw std::runtime_error();
    }
  }
  ~omx_jpg() { ilclient_destroy(client); }
  // std::pair<bool, cv::Mat>
  bool jpg_decode(unsigned char *src, std::size_t len) {
    IMAGE out;
    if (this->decodeImage(src, len, out))
      return false;

    void *fuck = new char[out.nData];
    memcpy(fuck, out.pData, out.nData);

    FILE *fp = fopen("test.yuv", "wb");
    fwrite(fuck, decoder->pOutputBufferHeader->nFilledLen, 1, fp);
    fclose(fp);
    printf("NAIVE\n");
    /*cv::Mat picYV12 = cv::Mat(720 * 3 / 2, 1280, CV_8UC1, fuck,
                              decoder->pOutputBufferHeader->nFilledLen);
    cv::Mat picBGR;
    cv::cvtColor(picYV12, picBGR, 99);
    // CV_YUV2BGR_YV12=99
    cv::imwrite("test.bmp", picBGR); // only for test*/

    /* decoder->pOutputBufferHeader->nFilledLen;
     decoder->pOutputBufferHeader->pBuffer;*/

    return true;
  }
  std::pair<bool, void *> jpg_encode(const cv::Mat &src) {}
};

} // namespace homemadecam

#endif