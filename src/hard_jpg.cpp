#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <utility>

#include "video/hard_jpg.h"

namespace hcam {

hard_jpg::hard_jpg() {
  bcm_host_init();
  int ret;
  ret = setupOpenMaxJpegDecoder(&pDecoder);
  assert(ret == 0);
}

hard_jpg::~hard_jpg() { ::cleanup(pDecoder); }

std::pair<bool, cv::Mat> hard_jpg::decode(unsigned char *src, uint32_t len) {
  if (0 != decodeImage(pDecoder, (char *)src, len)) {
    return std::pair<bool, cv::Mat>(false, cv::Mat());
  }

  OMX_PARAM_PORTDEFINITIONTYPE portdef;
  unsigned int uWidth, uHeight, nSize, nStride, nSliceHeight;

  // query output buffer requirements for resizer
  portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nVersion.nVersion = OMX_VERSION;
  portdef.nPortIndex = pDecoder->imageResizer->outPort;
  OMX_GetParameter(pDecoder->imageResizer->handle, OMX_IndexParamPortDefinition,
                   &portdef);

  uWidth = portdef.format.image.nFrameWidth;
  uHeight = portdef.format.image.nFrameHeight;
  nSliceHeight = portdef.format.image.nSliceHeight;
  nStride = portdef.format.image.nStride;
  nSize = portdef.nBufferSize;

  FILE *fp = fopen("haha.bgr", "wb");
  fwrite(pDecoder->pOutputBufferHeader->pBuffer, 1, nSize, fp);
  fclose(fp);

  abort();

  //这里没有拷贝buffer，注意他的生命周期
  cv::Mat bgr = cv::Mat::zeros(720, 1280, CV_8UC3);
  /*cv::imwrite("haha.jpg", bgr);*/

  // cv::Mat(720, 1280, CV_8UC1, pDecoder->pOutputBufferHeader->pBuffer);
  memcpy(bgr.data, pDecoder->pOutputBufferHeader->pBuffer, 1382400);

  return std::pair<bool, cv::Mat>(true, bgr);
}

} // namespace hcam
