#ifndef __HOMEMADECAM_V4L_CAPTURE_H__
#define __HOMEMADECAM_V4L_CAPTURE_H__
#include "logger.h"
#include <asm/types.h> /* for videodev2.h */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace homemadecam {

class v4l_capture {
  int fd;
  struct {
    void *start;
    size_t length;
  } buffer;

  void init() {
    fd = 0;
    memset(&buffer, 0, sizeof(buffer));
  }

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
      logger::error("VIDIOC_QUERYCAP failed");
      ::close(fd);
      init();
      return -2;
    }
    logger::info("DRIVER:", cap.driver, "(", cap.version, ")",
                 ", DEVICE:", cap.card);
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
      logger::error("V4L2_CAP_VIDEO_CAPTURE not supported");
      ::close(fd);
      init();
      return -2;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
      logger::error("V4L2_CAP_STREAMING not supported");
      ::close(fd);
      init();
      return -2;
    }

    struct v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.width = 1280;
    format.fmt.pix.height = 720;

    if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
      logger::error("VIDIOC_S_FMT failed");
      ::close(fd);
      init();
      return -3;
    }

    // allocate video memory for frame
    struct v4l2_requestbuffers buf_req;
    buf_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_req.memory = V4L2_MEMORY_MMAP;
    buf_req.count = 1;

    if (ioctl(fd, VIDIOC_REQBUFS, &buf_req) < 0) {
      logger::error("VIDIOC_REQBUFS failed");
      if (errno == EINVAL)
        logger::error("Video capturing or mmap-streaming is not supported\n");
      ::close(fd);
      init();
      return -4;
    }

    if (buf_req.count < 1) {
      logger::error("VIDIOC_REQBUFS failed");
      logger::error("no enough video memory");
      ::close(fd);
      init();
      return -4;
    }

    // map allocated video memory into our address space
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = bufrequest.type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf)) {
      ::close(fd);
      logger::error("VIDIOC_QUERYBUF failed");
      init();
      return -5;
    }

    buffer.length = buf.length; /* remember for munmap() */
    buffer.start =
        mmap(NULL, buf.length, PROT_READ | PROT_WRITE, /* recommended */
             MAP_SHARED,                               /* recommended */
             fd, buf.m.offset);

    if (MAP_FAILED == buffer.start) {
      ::close(fd);
      logger::error("mmap failed");
      init();
      return -6;
    }

    logger::info("successfully opened ", cap.card);
    return 0;
  }
  ~v4l_capture() { this->close(); }
  void read() {}
  void close() {
    if (fd)
      ::close(fd);
    if (buffer.start)
      munmap(buffer.start, buffer.length);
  }
};

} // namespace homemadecam

#endif