#ifdef __cplusplus
extern "C" {
#endif

#include <bcm_host.h>
#include <ilclient.h>

#ifdef __cplusplus
}
#endif
#include "omx/omx_lib.h"
#include <stdexcept>

namespace hcam {

omx_lib::omx_lib() {
  bcm_host_init();
  if (OMX_Init() != OMX_ErrorNone) {
    bcm_host_deinit();
    throw std::runtime_error(
        "Error init omx. There may be not enough gpu memory.");
  }
}
omx_lib::~omx_lib() {
  OMX_Deinit();
  bcm_host_deinit();
}

} // namespace hcam
