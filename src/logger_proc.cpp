#include "ipc/ipc.h"
#include "ipc/proc.h"
using namespace hcam;

void logger_proc(int ctl_fd) {
  while (true) {
    auto msg = ipc::recv(ctl_fd);
    if (msg.first) {
      logger::error("cap", "ipc handler read failed, quitting...", msg.first);
      raise(SIGTERM);
    }
    std::string text((char *)msg.second.content, msg.second.size);
    if (text == "PING") {
      //心跳
      if (ipc::send(ctl_fd, "PONG")) {
        // something goes wrong
        exit(SIGABRT);
      }
    } else if (text == "EXIT") {
      logger::debug("cap", "IPC EXIT, quitting...");
      ipc::child_exit(0);
    }
  }
}