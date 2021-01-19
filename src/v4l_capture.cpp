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

namespace hcam {

void v4l_capture::init() {
  fd = 0;
  memset(&_buffer, 0, sizeof(buffer) * V4L_BUFFER_CNT);
  first_frame = true;
}

bool v4l_capture::set_fps(uint32_t value) {
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

bool v4l_capture::check_cap(int32_t flags) {
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

bool v4l_capture::set_fmt(graphic g) {
  struct v4l2_format format;
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.pixelformat = codec_v4l2_pix_fmt(g.pix_fmt);
  format.fmt.pix.width = g.width;
  format.fmt.pix.height = g.height;
  format.fmt.pix.field = V4L2_FIELD_ANY;

  if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
    logger::error("VIDIOC_S_FMT failed");
    return false;
  }
  return true;
}

bool v4l_capture::setup_buffer() {
  struct v4l2_requestbuffers buf_req;
  buf_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf_req.memory = V4L2_MEMORY_MMAP;
  buf_req.count = V4L_BUFFER_CNT;

  if (ioctl(fd, VIDIOC_REQBUFS, &buf_req) < 0) {
    logger::error("VIDIOC_REQBUFS failed");
    if (errno == EINVAL)
      logger::error("Video capturing or mmap-streaming is not supported\n");
    return false;
  }

  if (buf_req.count < V4L_BUFFER_CNT) {
    logger::error("VIDIOC_REQBUFS failed");
    logger::error("no enough video memory");
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
      logger::error("VIDIOC_QUERYBUF failed");
      return false;
    }

    _buffer[i].length = buf.length; /* remember for munmap() */
    _buffer[i].data =
        mmap(NULL, buf.length, PROT_READ | PROT_WRITE, /* recommended */
             MAP_SHARED,                               /* recommended */
             fd, buf.m.offset);

    if (_buffer[i].data == MAP_FAILED) {
      logger::error("mmap failed");
      _buffer[i].data = nullptr;
      return false;
    }
  }

  // enable stream mode
  int type = buf_req.type;
  if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
    logger::error("stream on failed");
    return false;
  }

  return true;
}

bool v4l_capture::enqueue_buffer(uint32_t idx) {
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));

  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = idx;

  // clear the buffer(unnecessary but it wont cost much time)
  memset(_buffer[idx].data, 0, _buffer[idx].length);

  // Put the buffer in the incoming queue.
  if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
    return false;
  }
  return true;
}

bool v4l_capture::dequeue_buffer(uint32_t idx) {
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));

  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = idx;

  // The buffer's waiting in the outgoing queue.
  if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
    return false;
  }
  return true;
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
  auto gs = this->graphics(g.pix_fmt);
  {
    std::ostringstream oss;
    oss << "\r\n";
    for (const auto p : gs) {
      oss << codec_to_string(p.pix_fmt) << " ";
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

std::pair<bool, std::shared_ptr<v4l_capture::buffer>> v4l_capture::read() {
  // first time?
  // https://www.youtube.com/watch?v=qNgCGIBrK9A
  if (first_frame) {
    first_frame = false;
    if (!enqueue_buffer(next(&enq_idx))) {
      logger::error("enqueue_buffer failed");
      close();
      return std::pair<bool, std::shared_ptr<buffer>>(
          false, std::shared_ptr<buffer>());
    }
  }

  // get ready for the next frame
  if (!enqueue_buffer(next(&enq_idx))) {
    logger::error("enqueue_buffer failed");
    close();
    return std::pair<bool, std::shared_ptr<buffer>>(false,
                                                    std::shared_ptr<buffer>());
  }

  auto alloc_begin = checkpoint(3);

  // allocate space for return
  buffer *rv = (buffer *)malloc(sizeof(buffer) + _buffer[deq_idx].length);
  rv->data = (char *)rv + sizeof(buffer);
  rv->length = _buffer[deq_idx].length;

  auto alloc_end = checkpoint(3);

  // retrieve frame
  if (!dequeue_buffer(deq_idx)) {
    logger::error("dequeue_buffer failed");
    close();
    return std::pair<bool, std::shared_ptr<buffer>>(false,
                                                    std::shared_ptr<buffer>());
  }

  auto cpy_begin = checkpoint(3);
  // copy to return value
  memcpy(rv->data, this->_buffer[deq_idx].data, this->_buffer[deq_idx].length);
  auto cpy_end = checkpoint(3);

  next(&deq_idx);

  logger::info("v4lcapture alloc:", alloc_end - alloc_begin,
               "ms, cpy:", cpy_end - cpy_begin, "ms");

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
