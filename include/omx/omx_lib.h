#ifndef __HCAM_OMX_LIB_H__
#define __HCAM_OMX_LIB_H__

#ifdef __cplusplus
extern "C" {
#endif

//#include <OMX_Core>
#include <bcm_host.h>
#include <ilclient.h>

#ifdef __cplusplus
}
#endif

namespace hcam {

class omx_lib {
public:
  omx_lib() {
    bcm_host_init();
    if (OMX_Init() != OMX_ErrorNone) {
      bcm_host_deinit();
      throw std::runtime_error(
          "Error init omx. There may be not enough gpu memory.");
    }
  }
  ~omx_lib() {
    OMX_Deinit();
    bcm_host_deinit();
  }
};

} // namespace hcam

#endif