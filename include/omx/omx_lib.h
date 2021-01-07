#ifndef __HOMEMADECAM_OMX_UTIL_H__
#define __HOMEMADECAM_OMX_UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <OMX_Core>
#include <bcm_host.h>
#include <ilclient.h>

#ifdef __cplusplus
}
#endif

namespace homemadecam {

class omx_lib {
public:
  omx_lib() {
    bcm_host_init();
    if (OMX_Init() != OMX_ErrorNone) {
      fprintf(stderr, "Error init omx. There may be not enough gpu memory.\n");
      bcm_host_deinit();
      throw std::runtime_error();
    }
  }
  ~omx_lib() {
    OMX_Deinit();
    bcm_host_deinit();
  }
};

} // namespace homemadecam

#endif