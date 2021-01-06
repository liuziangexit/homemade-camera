#ifndef __HOMEMADECAM_V4L_CAPTURE_H__
#define __HOMEMADECAM_V4L_CAPTURE_H__
#include "logger.h"
#include <asm/types.h> /* for videodev2.h */
#include <fcntl.h>
#include <linux/videodev2.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace homemadecam {

class v4l_capture {
  int fd;

  void init() { fd = 0; }

public:
  v4l_capture() { init(); }
  int open(const std::string &device) {
    // open device
    if ((fd = ::open(device.c_str(), O_RDWR)) < 0) {
      init();
      return -1;
    }
    // query device capabilities
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
      init();
      return -2;
    }
    logger::info("DRIVER:", cap.driver, "(", cap.version, ")",
                 ", DEVICE:", cap.card);
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
      logger::error("V4L2_CAP_VIDEO_CAPTURE not supported");
      init();
      return -2;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
      logger::error("V4L2_CAP_STREAMING not supported");
      init();
      return -2;
    }

    logger::info("successfully opened ", cap.card);
    return 0;
  }
  ~v4l_capture() { this->close(); }
  void read() {}
  void close() {
    if (fd)
      ::close(fd);
  }
};

} // namespace homemadecam

#endif