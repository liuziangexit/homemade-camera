#include "ipc/ipc.h"
#include <assert.h>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//理论上说这punning是个ub，但who cares

int hcam::send_msg(int fd, const char *content) {
  return hcam::send_msg(fd, (unsigned char *)content,
                        (uint32_t)strlen(content));
}

int hcam::send_msg(int fd, unsigned char *content, uint32_t len) {
  ssize_t written;
  // 1.写length
  static_assert(sizeof(uint32_t) == sizeof(char) * 4, "what?");
  union {
    unsigned char plain[4];
    uint32_t integer;
  } punning;
  memset(&punning, 0, 4);
  punning.integer = len;
  written = write(fd, punning.plain, 4);
  if (written != 4) {
    return -1;
  }
  // 2.写content
  written = write(fd, content, len);
  return written == len ? 0 : -1;
}

std::pair<int, hcam::message> hcam::recv_msg(int fd) {
  static_assert(sizeof(uint32_t) == sizeof(char) * 4, "what?");
  union {
    unsigned char plain[4];
    uint32_t integer;
  } punning;
  memset(&punning, 0, 4);
  // 1.读长度
  int total_transferred = 0;
  while (total_transferred < 4) {
    int transferred = read(fd, //
                           (unsigned char *)punning.plain + total_transferred,
                           4 - total_transferred);
    if (transferred == 0) {
      // sock关了?
      return std::pair<bool, hcam::message>{-2, hcam::message()};
    }
    total_transferred += transferred;
  }
  assert(total_transferred == 4);
  // 2.读内容
  unsigned char *content =
      static_cast<unsigned char *>(malloc(punning.integer));
  if (!content) {
    throw std::bad_alloc();
  }
  hcam::message ret;
  ret.content = content;
  ret.capacity = punning.integer;
  ret.size = punning.integer;

  total_transferred = 0;
  while (total_transferred < punning.integer) {
    int transferred =
        read(fd, //
             content + total_transferred, punning.integer - total_transferred);
    if (transferred == 0) {
      // sock关了?
      return std::pair<bool, hcam::message>{-1, hcam::message()};
    }
    total_transferred += transferred;
  }
  assert(total_transferred == punning.integer);
  return std::pair<bool, hcam::message>(0, ret);
}

hcam::message::message() : size(0), capacity(0), content(nullptr) {}

hcam::message::message(const message &rhs) {
  this->content = static_cast<unsigned char *>(malloc(rhs.size));
  if (!this->content) {
    throw std::bad_alloc();
  }
  memcpy(this->content, rhs.content, rhs.size);
  this->capacity = rhs.size;
  this->size = rhs.size;
}

hcam::message::message(message &&rhs) {
  this->content = rhs.content;
  this->size = rhs.size;
  this->capacity = rhs.capacity;
  new (&rhs) hcam::message();
}

hcam::message::~message() {
  if (this->content) {
    free(this->content);
  }
}

int hcam::wait_msg(int fd, uint32_t timeout) {
  struct pollfd fds;
  memset(&fds, 0, sizeof(fds));
  fds.fd = fd;
  fds.events = POLLIN;
  int ret;
  while (true) {
    ret = poll(&fds, 1, timeout);
    if (ret == -1) {
      if (errno == EINTR)
        continue;
      perror("poll() failed");
      return -1;
    } else if (ret == 0) {
      // timeout
      return 0;
    } else if (fds.revents & POLLIN) {
      return 1;
    } else if (fds.revents & (POLLERR | POLLNVAL)) {
      return -2;
    }
  }
}
