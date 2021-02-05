#include "video/v4l_capture.h"
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
#include <vector>
#include <video/codec.h>

#define BUFFER_TYPE V4L2_BUF_TYPE_VIDEO_CAPTURE

namespace hcam {

void v4l_capture::init() {
  fd = 0;
  memset(&_buffer, 0, sizeof(buffer) * V4L_BUFFER_CNT);
}

bool v4l_capture::set_fps(uint32_t value) {
  v4l2_streamparm streamparm;
  memset(&streamparm, 0, sizeof(v4l2_streamparm));
  streamparm.type = BUFFER_TYPE;
  if (ioctl(fd, VIDIOC_G_PARM, &streamparm) != 0) {
    logger::error("cap", "VIDIOC_G_PARM failed");
    return false;
  }
  if (!(streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
    logger::error("cap", "V4L2_CAP_TIMEPERFRAME not supported");
    return false;
  }

  streamparm.parm.capture.timeperframe.numerator = 1;
  streamparm.parm.capture.timeperframe.denominator = value;
  if (ioctl(fd, VIDIOC_S_PARM, &streamparm) != 0) {
    logger::error("cap", "VIDIOC_S_PARM failed");
    return false;
  }
  logger::debug("cap", "fps are now set to ",
                streamparm.parm.capture.timeperframe.denominator);
  return true;
}

bool v4l_capture::check_cap(int32_t flags) {
  struct v4l2_capability cap;
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
    logger::error("cap", "VIDIOC_QUERYCAP failed");
    return false;
  }
  logger::info("cap", "DRIVER:", cap.driver, "(", cap.version, ")",
               ", DEVICE:", cap.card);
  if (!(cap.capabilities & flags)) {
    logger::error("cap", "feature not supported");
    return false;
  }
  return true;
}

bool v4l_capture::set_fmt(graphic g) {
  struct v4l2_format format;
  format.type = BUFFER_TYPE;
  format.fmt.pix.pixelformat = codec_v4l2_pix_fmt(g.pix_fmt);
  format.fmt.pix.width = g.width;
  format.fmt.pix.height = g.height;
  format.fmt.pix.field = V4L2_FIELD_ANY;

  if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
    logger::error("cap", "VIDIOC_S_FMT failed");
    return false;
  }
  return true;
}

bool v4l_capture::setup_buffer() {
  struct v4l2_requestbuffers buf_req;
  buf_req.type = BUFFER_TYPE;
  buf_req.memory = V4L2_MEMORY_MMAP;
  buf_req.count = V4L_BUFFER_CNT;

  if (ioctl(fd, VIDIOC_REQBUFS, &buf_req) < 0) {
    logger::error("cap", "VIDIOC_REQBUFS failed");
    if (errno == EINVAL)
      logger::error("cap",
                    "Video capturing or mmap-streaming is not supported\n");
    return false;
  }

  if (buf_req.count < V4L_BUFFER_CNT) {
    logger::error("cap", "VIDIOC_REQBUFS failed");
    logger::error("cap", "no enough video memory");
    return false;
  }

  //循环的时候如果出错了，那需要把此前循环时候map的内存都unmap回去，这个工作会在此函数返回false之后的一个close里做
  // map the allocated video memory into the program's address space
  for (int i = 0; i < V4L_BUFFER_CNT; i++) {
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = buf_req.type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf)) {
      logger::error("cap", "VIDIOC_QUERYBUF failed");
      return false;
    }

    _buffer[i].length = buf.length; /* remember for munmap() */
    _buffer[i].data =
        mmap(NULL, buf.length, PROT_READ | PROT_WRITE, /* recommended */
             MAP_SHARED,                               /* recommended */
             fd, buf.m.offset);

    if (_buffer[i].data == MAP_FAILED) {
      logger::error("cap", "mmap failed");
      _buffer[i].data = nullptr;
      return false;
    }
  }

  return true;
}

bool v4l_capture::stream_on() {
  // enable stream mode
  int type = BUFFER_TYPE;
  if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
    logger::error("cap", "stream on failed");
    return false;
  }
  return true;
}

bool v4l_capture::enqueue_buffer(uint32_t idx) {
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));

  buf.type = BUFFER_TYPE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = idx;

  // Put the buffer in the incoming queue.
  if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
    return false;
  }
  logger::debug("cap", "v4l_capture::enqueue_buffer <- ", idx);
  return true;
}

int v4l_capture::dequeue_buffer() {
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));

  buf.type = BUFFER_TYPE;
  buf.memory = V4L2_MEMORY_MMAP;

  // The buffer's waiting in the outgoing queue.
  int ret;
  fd_set fds;
  struct timeval tv;

  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  /* Timeout. */
  tv.tv_sec = 2;
  tv.tv_usec = 0;

  while (true) {
    ret = select(fd + 1, &fds, NULL, NULL, &tv);
    if (ret == -1) {
      if (EINTR == errno)
        continue;
      return ret;
    }
    if (ret == 0) {
      logger::info("cap", "v4l select timeout");
      return -999;
    }
    break;
  }
  ret = ioctl(fd, VIDIOC_DQBUF, &buf);

  /*if (buf.flags & V4L2_BUF_FLAG_ERROR != 0) {
  logger::error("cap", "v4l_capture::dequeue_buffer V4L2_BUF_FLAG_ERROR");
   }*/
  if (ret < 0) {
    return -998;
  }
  logger::debug("cap", "v4l_capture::dequeue_buffer -> ", buf.index);
  return buf.index;
}

v4l_capture::v4l_capture() { init(); }

v4l_capture::~v4l_capture() { this->close(); }

std::vector<v4l_capture::graphic> v4l_capture::graphics(codec pix_fmt) {
  if (this->fd < 0) {
    throw std::runtime_error("invalid fd");
  }

  std::vector<graphic> rv;
  struct v4l2_frmsizeenum frmsize;
  memset(&frmsize, 0, sizeof(frmsize));
  frmsize.pixel_format = codec_v4l2_pix_fmt(pix_fmt);
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
                               (double)frmival.discrete.numerator),
                    pix_fmt};
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

int v4l_capture::open(const std::string &device, v4l_capture::graphic g) {
  auto fail = [this](const char *error_message, int ret) -> int {
    logger::error("cap", error_message);
    close();
    return ret;
  };

  // open device
  if ((fd = ::open(device.c_str(), O_RDWR)) < 0) {
    return fail("v4l_capture open device failed", -1);
  }

  // check device capabilities
  if (!check_cap(V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE)) {
    return fail("v4l_capture device capabilities failed", -2);
  }

  // check if the specified graphic will working
  auto gs = this->graphics(g.pix_fmt);
  {
    std::ostringstream oss;
    oss << "\r\n";
    for (const auto p : gs) {
      oss << codec_to_string(p.pix_fmt) << " ";
      oss << p.width << "x" << p.height << "@" << p.fps << std::endl;
    }
    logger::info("cap", "available graphics on this device are:", oss.str());
  }

  if (std::find(gs.begin(), gs.end(), g) == gs.end()) {
    return fail(
        "v4l_capture specified graphic is not applicable on this device", -3);
  }

  // set format
  if (!set_fmt(g)) {
    return fail("v4l_capture set fmt failed", -4);
  }

  // fps
  if (!set_fps(g.fps)) {
    return fail("v4l_capture set fps failed", -5);
  }

  // allocate video memory for frame
  if (!setup_buffer()) {
    return fail("v4l_capture setup buffer failed", -6);
  }

  for (int i = 0; i < V4L_BUFFER_CNT; i++) {
    if (!enqueue_buffer(i)) {
      logger::error("cap", "enqueue_buffer failed");
      close();
      return fail("v4l_capture prepare buffer failed", -7);
    }
  }

  if (!stream_on()) {
    return fail("v4l_capture stream on failed", -8);
  }

  logger::info("cap", "device been successfully opened");

  return 0;
}

std::pair<bool, std::shared_ptr<v4l_capture::buffer>> v4l_capture::read() {
  // allocate space for return
  auto alloc_time = checkpoint(3);
  buffer *rv = (buffer *)malloc(sizeof(buffer) + _buffer[0].length);
  rv->data = (char *)rv + sizeof(struct buffer);
  rv->length = _buffer[0].length;

  // retrieve frame
  auto dequeue_time = checkpoint(3);
  int buffer_index;
  if ((buffer_index = dequeue_buffer()) < 0) {
    logger::error("cap", "dequeue_buffer failed");
    close();
    return std::pair<bool, std::shared_ptr<buffer>>(false,
                                                    std::shared_ptr<buffer>());
  }

  /*
   * 这里的memcpy特别慢，按照目前的分析，原因是...
   * 1)这内存是摄像头硬件里的内存，被我们mmap进了程序地址空间，所以实际上当我们访问它的时候，访问的是摄像头硬件，这肯定很慢
   * 2)因为这内存不属于主存，所以没有被CPU的cache缓存，所以很慢
   */

  // copy to return value
  auto copy_time = checkpoint(3);
  memcpy(rv->data, this->_buffer[buffer_index].data, this->_buffer[0].length);

  // get ready for the next frame
  auto enqueue_time = checkpoint(3);
  if (!enqueue_buffer(buffer_index)) {
    logger::error("cap", "enqueue_buffer failed");
    close();
    return std::pair<bool, std::shared_ptr<buffer>>(false,
                                                    std::shared_ptr<buffer>());
  }

  //光线弱的时候会掉帧，据说，这是驱动有意延长曝光时间导致的
  auto done_time = checkpoint(3);
  logger::debug("cap", "v4lcapture alloc:", dequeue_time - alloc_time,
                "ms, dequeue:", copy_time - dequeue_time,
                "ms, copy:", enqueue_time - copy_time,
                "ms, enqueue:", done_time - enqueue_time, "ms");

  return std::pair<bool, std::shared_ptr<buffer>>(
      true, std::shared_ptr<buffer>(rv, [](buffer *p) { free(p); }));
}

void v4l_capture::close() {
  for (int i = 0; i < V4L_BUFFER_CNT; i++) {
    if (_buffer[i].data) {
      munmap(_buffer[i].data, _buffer[i].length);
    }
  }
  if (fd) {
    ::close(fd);
  }
  init();
}

} // namespace hcam
