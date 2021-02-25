#include "ipc/proc.h"
#include "util/logger.h"
#include "video/capture.h"

void cap_proc(int ctl_fd, int log_fd, int cap_fd) {
  hcam::logger::start_logger(log_fd);
  cv::setNumThreads(hcam::config::get().video_thread_count);
  hcam::capture *cap = new hcam::capture(cap_fd);
  cap->run(ctl_fd);
  delete cap;
  hcam::ipc::child_exit(0);
}