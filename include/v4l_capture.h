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
    logger::info("driver:", cap.driver, "(", cap.version, ")",
                 ", device:", cap.card, "@", cap.bus_info);
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
      init();
      return -2;
    }
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