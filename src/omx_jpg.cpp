#include "omx/omx_jpg.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "omx/omx_image.h"

#ifdef __cplusplus
}
#endif

#include "omx/image_def.h"
#include "util/guard.h"
#include "util/logger.h"
#include <assert.h>
#include <opencv2/core/core_c.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stddef.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <util/time_util.h>
#include <utility>

namespace hcam {

int omx_jpg::decodeImage(const unsigned char *src, uint32_t len, IMAGE *image) {
  FILE *imageFile = fmemopen((void *)src, len, "rb");

  if (!imageFile) {
    return -1;
  }

  unsigned char magNum[8];
  if (fread(&magNum, 1, 8, imageFile) != 8) {
    fclose(imageFile);
    return -2;
  }
  rewind(imageFile);

  int ret;
  static const unsigned char magNumJpeg[] = {0xff, 0xd8, 0xff};
  if (memcmp(magNum, magNumJpeg, sizeof(magNumJpeg)) == 0) {
    /*  if (soft || jInfo.mode == JPEG_MODE_PROGRESSIVE ||
          jInfo.nColorComponents != 3) {
        if (info)
          printf("Soft decode jpeg\n");
        ret = softDecodeJpeg(imageFile, image);
      } else {*/
    ret = omxDecodeJpeg(client, imageFile, image);
  } else {
    logger::error("Unsupported image");
    fclose(imageFile);
    return -3;
  }

  fclose(imageFile);

  /*logger::info("decodeImage: Width: ", image->width,
               ", Height: ", image->height);*/

  return ret;
}

omx_jpg::omx_jpg() {
  if ((client = ilclient_init()) == NULL) {
    throw std::runtime_error("Error init ilclient");
  }
}
omx_jpg::~omx_jpg() { ilclient_destroy(client); }

std::pair<bool, cv::Mat> omx_jpg::decode(unsigned char *src, uint32_t len) {
  // OMX decode jpeg
  IMAGE out;
  auto decode = checkpoint(3);
  if (this->decodeImage(src, len, &out))
    return std::pair<bool, cv::Mat>(false, cv::Mat());
  guard gout = [&out] { destroyImage(&out); };

  // convert yuv420 to bgr
  cv::Mat yuv420 = cv::Mat(out.height * 3 / 2, out.width, CV_8UC1, out.pData);
  cv::Mat bgr(out.height, out.width, CV_8UC3);
  auto cvtcolor = checkpoint(3);
  cv::cvtColor(yuv420, bgr, cv::COLOR_YUV2BGR_I420);
  auto done = checkpoint(3);
  /*
    logger::info("omx_jpg decode:", cvtcolor - decode,
                 "ms, cvtcolor:", done - cvtcolor, "ms");*/

  return std::pair<bool, cv::Mat>(true, bgr);
}

std::pair<bool, void *> omx_jpg::encode(const cv::Mat &src) {
  throw std::runtime_error("encode not available");
}
} // namespace hcam