#include "util/logger.h"
#include <mutex>

namespace hcam {
std::mutex logger::mut;
std::condition_variable logger::cv;
std::queue<logger::log> logger::log_queue;
} // namespace hcam
