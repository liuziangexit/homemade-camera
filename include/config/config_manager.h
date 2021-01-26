#ifndef __HCAM_CONFIG_MANAGER_H__
#define __HCAM_CONFIG_MANAGER_H__

#include "config.h"
#include "lazy/lazy.h"
#include <string>

namespace hcam {

class config_manager {
  static liuziangexit_lazy::lazy_t<config, std::string> c;

public:
  static config get() { return c.get_instance(); }
};

} // namespace hcam

#endif
