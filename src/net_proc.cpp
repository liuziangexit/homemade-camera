#include "ipc/proc.h"
#include "util/logger.h"
#include "web/web.h"

int net_to_ctl;
using namespace hcam;

void net_proc(int ctl_fd, int log_fd, int cap_fd, int _net_ctl) {
  net_to_ctl = _net_ctl;
  hcam::logger::start_logger(log_fd);
  hcam::web *web = new hcam::web(cap_fd);
  web->run(ctl_fd);
  delete web;
  hcam::ipc::child_exit(0);
}

void net_reload() { ipc::send(net_to_ctl, "RELOAD CONFIG"); }