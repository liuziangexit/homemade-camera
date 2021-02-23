//
// Created by 刘子昂 on 2021/2/23.
//

#ifndef HOMECAM_IPC_H
#define HOMECAM_IPC_H
#include <cstdint>
#include <memory>
#include <stdlib.h>
#include <sys/socket.h>
#include <utility>

namespace hcam {

bool send_msg(int fd, unsigned char *content, uint32_t len);
struct message {
  uint32_t size;
  uint32_t capacity;
  unsigned char *content;

  message();
  message(const message &);
  message(message &&);
  ~message();
};
std::pair<bool, message> recv_msg(int fd);

} // namespace hcam

#endif // HOMECAM_IPC_H
