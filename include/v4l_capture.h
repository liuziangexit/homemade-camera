#ifndef __HOMEMADECAM_V4L_CAPTURE_H__
#define __HOMEMADECAM_V4L_CAPTURE_H__
#include "logger.h"
#include "time_util.h"
#include <asm/types.h> /* for videodev2.h */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <memory>
#include <stdlib.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace homemadecam {

class v4l_capture {
public:
  struct buffer {
    void *data;
    std::size_t length;
  };

private:
  int fd;
  buffer _buffer;
  uint32_t last_read;

  void init() {
    fd = 0;
    memset(&_buffer, 0, sizeof(buffer));
    last_read = 0;
  }

  bool set_fps(uint32_t value) {
    v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(v4l2_streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_PARM, &streamparm) != 0) {
      logger::error("VIDIOC_G_PARM failed");
      return false;
    }
    if (streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
      logger::error("V4L2_CAP_TIMEPERFRAME not supported");
      return false;
    }

    /* v4l2_frmivalenum frmivalenum;
     memset(&frmivalenum, 0, sizeof(v4l2_frmivalenum));
     if (!ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmivalenum)) {
     }*/

    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = value;
    if (!ioctl(fd, VIDIOC_S_PARM, &streamparm)) {
      logger::error("VIDIOC_S_PARM failed");
      return false;
    }
    return true;
  }

public:
  v4l_capture() { init(); }
  int open(const std::string &device, uint32_t fps) {
    // open device
    if ((fd = ::open(device.c_str(), O_RDWR)) < 0) {
      close();
      return -1;
    }
    // query device capabilities
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
      logger::error("VIDIOC_QUERYCAP failed");
      close();
      return -2;
    }
    logger::info("DRIVER:", cap.driver, "(", cap.version, ")",
                 ", DEVICE:", cap.card);
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
      logger::error("V4L2_CAP_VIDEO_CAPTURE not supported");
      close();
      return -2;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
      logger::error("V4L2_CAP_STREAMING not supported");
      close();
      return -2;
    }

    struct v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.width = 1280;
    format.fmt.pix.height = 720;

    if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
      logger::error("VIDIOC_S_FMT failed");
      close();
      return -3;
    }

    if (!set_fps(fps)) {
      logger::error("setfps failed");
      close();
      return -99;
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
      close();
      return -4;
    }

    if (buf_req.count < 1) {
      logger::error("VIDIOC_REQBUFS failed");
      logger::error("no enough video memory");
      close();
      return -4;
    }

    // map allocated video memory into our address space
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = buf_req.type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf)) {
      logger::error("VIDIOC_QUERYBUF failed");
      close();
      return -5;
    }

    _buffer.length = buf.length; /* remember for munmap() */
    _buffer.data =
        mmap(NULL, buf.length, PROT_READ | PROT_WRITE, /* recommended */
             MAP_SHARED,                               /* recommended */
             fd, buf.m.offset);

    if (MAP_FAILED == _buffer.data) {
      logger::error("mmap failed");
      close();
      return -6;
    }

    // enable stream mode
    int type = buf.type;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
      logger::error("stream on failed");
      close();
      return -7;
    }

    logger::info("successfully opened ", cap.card);
    return 0;
  }
  ~v4l_capture() { this->close(); }

  std::pair<bool, std::shared_ptr<buffer>> read() {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    /* Here is where you typically start two loops:
     * - One which runs for as long as you want to
     *   capture frames (shoot the video).
     * - One which iterates over your buffers everytime. */

    // Put the buffer in the incoming queue.
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
      logger::error("VIDIOC_QBUF failed");
      close();
      return std::pair<bool, std::shared_ptr<buffer>>(
          false, std::shared_ptr<buffer>());
    }

    // The buffer's waiting in the outgoing queue.
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
      logger::error("VIDIOC_DQBUF failed");
      close();
      return std::pair<bool, std::shared_ptr<buffer>>(
          false, std::shared_ptr<buffer>());
    }

    uint32_t current = checkpoint(3);
    if (last_read == 0) {
      logger::info("read frame ok");
    } else {
      logger::info("read frame ok, interval: ", current - last_read, "ms");
    }
    last_read = current;

    /* Your loops end here. */

    buffer *rv = (buffer *)malloc(sizeof(buffer) + _buffer.length);
    rv->data = (char *)rv + sizeof(buffer);
    rv->length = _buffer.length;

    memcpy(rv->data, this->_buffer.data, this->_buffer.length);

    return std::pair<bool, std::shared_ptr<buffer>>(
        true, std::shared_ptr<buffer>(rv, [](buffer *p) { free(p); }));
  }

  void close() {
    if (fd)
      ::close(fd);
    if (_buffer.data)
      munmap(_buffer.data, _buffer.length);
    init();
  }
};

} // namespace homemadecam

#endif