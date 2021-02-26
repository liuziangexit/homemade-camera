#include "util/logger.h"
#include <mutex>

namespace hcam {
logger::logger_ctx *logger::context = nullptr;
alignas(logger::logger_ctx) unsigned char logger::context_place[sizeof(
    logger::logger_ctx)];
} // namespace hcam
