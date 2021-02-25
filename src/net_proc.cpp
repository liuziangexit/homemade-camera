#include "ipc/proc.h"
#include "web/web.h"

void net_proc(int ctl_fd, int cap_fd) {
  hcam::web *web = new hcam::web(cap_fd);
  web->run(ctl_fd);
  delete web;
  hcam::ipc::child_exit(0);
}