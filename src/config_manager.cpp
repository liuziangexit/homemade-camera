#include "config/config_manager.h"

namespace hcam {

liuziangexit_lazy::lazy_t<config, std::string> config_manager::c =
    liuziangexit_lazy::make_lazy<config, std::string>("config.json");

} // namespace hcam
