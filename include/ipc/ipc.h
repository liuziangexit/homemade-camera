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

int send_msg(int fd, const char *content);
int send_msg(int fd, unsigned char *content, uint32_t len);
struct message {
  uint32_t size;
  uint32_t capacity;
  unsigned char *content;

  message();
  message(const message &);
  message(message &&);
  ~message();
};
std::pair<int, message> recv_msg(int fd);
//timeout为负数则表示没有timeout，为0表示立即返回，大于0为毫秒
//返回负数表示错误，返回0表示没有，返回1表示有
int wait_msg(int fd, int32_t timeout);

} // namespace hcam

#endif // HOMECAM_IPC_H
