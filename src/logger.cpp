#include "util/logger.h"
#include <mutex>

namespace hcam {
static bool quit = false;
std::mutex logger::mut;
std::condition_variable logger::cv;
std::queue<logger::log> logger::log_queue;
} // namespace hcam
