#ifndef __HCAM_V4L_CAPTURE_H__
#define __HCAM_V4L_CAPTURE_H__
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

#define V4L_BUFFER_CNT 2

namespace hcam {

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
  buffer _buffer[V4L_BUFFER_CNT];
  uint32_t last_read;
  bool first_frame;
  int enq_idx = 0, deq_idx = 0;

  void init();
  bool set_fps(uint32_t);
  bool check_cap(int32_t);
  bool set_fmt(graphic);
  bool setup_buffer();
  bool enqueue_buffer(uint32_t);
  bool dequeue_buffer(uint32_t);
  inline int next(int *val) {
    auto ret = *val;
    *val = (*val + 1) % V4L_BUFFER_CNT;
    return ret;
  }

public:
  v4l_capture();
  ~v4l_capture();
  std::vector<graphic> graphics();
  int open(const std::string &, graphic);
  std::pair<bool, std::shared_ptr<buffer>> read();
  void close();
};

} // namespace hcam

#endif
