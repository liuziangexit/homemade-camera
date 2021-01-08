#ifndef __HOMEMADECAM_V4L_CAPTURE_H__
#define __HOMEMADECAM_V4L_CAPTURE_H__
#include "util/logger.h"
#include "util/time_util.h"
#include <algorithm>
#include <asm/types.h> /* for videodev2.h */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <memory>
#include <sstream>
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

  // TODO 支持设定像素格式yuv/mjpg
  struct graphic {
    uint32_t width;
    uint32_t height;
    uint32_t fps;

    bool operator==(const graphic &rhs) const {
      return width == rhs.width && height == rhs.height && fps == rhs.fps;
    }
  };

private:
  int fd;
  buffer _buffer;
  uint32_t last_read;
  bool enqueue;

  void init() {
    fd = 0;
    memset(&_buffer, 0, sizeof(buffer));
    last_read = 0;
    enqueue = false;
  }

  bool set_fps(uint32_t value) {
    v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(v4l2_streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_PARM, &streamparm) != 0) {
      logger::error("VIDIOC_G_PARM failed");
      return false;
    }
    if (!(streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
      logger::error("V4L2_CAP_TIMEPERFRAME not supported");
      return false;
    }

    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = value;
    if (ioctl(fd, VIDIOC_S_PARM, &streamparm) != 0) {
      logger::error("VIDIOC_S_PARM failed");
      return false;
    }
    logger::info("fps are now set to ",
                 streamparm.parm.capture.timeperframe.denominator);
    return true;
  }

  bool check_cap(int32_t flags) {
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
      logger::error("VIDIOC_QUERYCAP failed");
      return false;
    }
    logger::info("DRIVER:", cap.driver, "(", cap.version, ")",
                 ", DEVICE:", cap.card);
    if (!(cap.capabilities & flags)) {
      logger::error("feature not supported");
      return false;
    }
    return true;
  }

  bool set_fmt(graphic g) {
    struct v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.width = g.width;
    format.fmt.pix.height = g.height;
    format.fmt.pix.field = V4L2_FIELD_ANY;

    if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
      logger::error("VIDIOC_S_FMT failed");
      return false;
    }
    return true;
  }

  bool setup_buffer() {
    struct v4l2_requestbuffers buf_req;
    buf_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_req.memory = V4L2_MEMORY_MMAP;
    buf_req.count = 1;

    if (ioctl(fd, VIDIOC_REQBUFS, &buf_req) < 0) {
      logger::error("VIDIOC_REQBUFS failed");
      if (errno == EINVAL)
        logger::error("Video capturing or mmap-streaming is not supported\n");
      return false;
    }

    if (buf_req.count < 1) {
      logger::error("VIDIOC_REQBUFS failed");
      logger::error("no enough video memory");
      return false;
    }

    // map allocated video memory into our address space
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = buf_req.type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf)) {
      logger::error("VIDIOC_QUERYBUF failed");
      return false;
    }

    _buffer.length = buf.length; /* remember for munmap() */
    _buffer.data =
        mmap(NULL, buf.length, PROT_READ | PROT_WRITE, /* recommended */
             MAP_SHARED,                               /* recommended */
             fd, buf.m.offset);

    if (MAP_FAILED == _buffer.data) {
      logger::error("mmap failed");
      return false;
    }

    // enable stream mode
    int type = buf.type;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
      logger::error("stream on failed");
      return false;
    }

    return true;
  }

  bool enqueue_buffer() {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    // clear the buffer(unnecessary but it wont cost much time)
    memset(_buffer.data, 0, _buffer.length);

    // Put the buffer in the incoming queue.
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
      return false;
    }
    return true;
  }

  bool dequeue_buffer() {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    // The buffer's waiting in the outgoing queue.
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
      return false;
    }
    return true;
  }

public:
  v4l_capture() { init(); }

  std::vector<graphic> graphics() {
    if (this->fd < 0) {
      throw std::runtime_error("invalid fd");
    }

    std::vector<graphic> rv;
    struct v4l2_frmsizeenum frmsize;
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.pixel_format = V4L2_PIX_FMT_MJPEG;
    while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
      if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        struct v4l2_frmivalenum frmival;
        memset(&frmival, 0, sizeof(frmival));
        frmival.pixel_format = frmsize.pixel_format;
        frmival.width = frmsize.discrete.width;
        frmival.height = frmsize.discrete.height;
        while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
          if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            graphic g{frmsize.discrete.width, frmsize.discrete.height,
                      (uint32_t)((double)frmival.discrete.denominator /
                                 (double)frmival.discrete.numerator)};
            rv.push_back(g);
          } else {
            throw std::runtime_error("unsupported v4l2_frmivalenum.type");
          }
          frmival.index++;
        }
      } else {
        throw std::runtime_error("unsupported v4l2_frmsizeenum.type");
      }
      frmsize.index++;
    }
    return rv;
  }

  int open(const std::string &device, graphic g) {
    // open device
    if ((fd = ::open(device.c_str(), O_RDWR)) < 0) {
      close();
      return -1;
    }

    // check device capabilities
    if (!check_cap(V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE)) {
      close();
      return -2;
    }

    // check if the specified graphic will working
    auto gs = this->graphics();
    {
      std::ostringstream oss;
      oss << "\r\n";
      for (const auto p : gs) {
        oss << p.width << "x" << p.height << "@" << p.fps << std::endl;
      }
      logger::info("available graphics on this device are:", oss.str());
    }

    if (std::find(gs.begin(), gs.end(), g) == gs.end()) {
      logger::error("specified graphic is not applicable on this device");
      close();
      return -3;
    }

    // set format
    if (!set_fmt(g)) {
      logger::error("setfmt failed");
      close();
      return -4;
    }

    // fps
    if (!set_fps(g.fps)) {
      logger::error("setfps failed");
      close();
      return -5;
    }

    // allocate video memory for frame
    if (!setup_buffer()) {
      logger::error("setup buffer failed");
      close();
      return -6;
    }

    logger::info("device been successfully opened");
    return 0;
  }

  ~v4l_capture() { this->close(); }

  std::pair<bool, std::shared_ptr<buffer>> read() {
    // first time?
    // https://www.youtube.com/watch?v=qNgCGIBrK9A
    if (!enqueue) {
      enqueue = true;
      if (!enqueue_buffer()) {
        logger::error("enqueue_buffer failed");
        close();
        return std::pair<bool, std::shared_ptr<buffer>>(
            false, std::shared_ptr<buffer>());
      }
    }

    auto alloc_begin = checkpoint(3);

    // allocate space for refturn
    buffer *rv = (buffer *)malloc(sizeof(buffer) + _buffer.length);
    rv->data = (char *)rv + sizeof(buffer);
    rv->length = _buffer.length;

    auto alloc_end = checkpoint(3);

    // retrieve frame
    if (!dequeue_buffer()) {
      logger::error("dequeue_buffer failed");
      close();
      return std::pair<bool, std::shared_ptr<buffer>>(
          false, std::shared_ptr<buffer>());
    }

    // FIXME deq之后，enq之前，这个地方如果摄像头产出了一个新帧的话，就丢失了

    auto cpy_begin = checkpoint(3);
    // copy to return value
    memcpy(rv->data, this->_buffer.data, this->_buffer.length);
    auto cpy_end = checkpoint(3);

    logger::info("v4lcapture alloc:", alloc_end - alloc_begin,
                 "ms, cpy:", cpy_end - cpy_begin, "ms");

    // get ready for the next frame
    if (!enqueue_buffer()) {
      logger::error("enqueue_buffer failed");
      close();
      return std::pair<bool, std::shared_ptr<buffer>>(
          false, std::shared_ptr<buffer>());
    }

    return std::pair<bool, std::shared_ptr<buffer>>(
        true, std::shared_ptr<buffer>(rv, [](buffer *p) { free(p); }));
  }

  void close() {
    if (_buffer.data)
      munmap(_buffer.data, _buffer.length);
    if (fd)
      ::close(fd);
    init();
  }
};

} // namespace homemadecam

#endif
