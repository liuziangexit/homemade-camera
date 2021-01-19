#include "util/logger.h"
#include <mutex>

namespace hcam {
std::mutex logger::mut;
} // namespace hcam
