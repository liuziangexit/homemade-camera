//
// Created by 刘子昂 on 2021/2/23.
//

#include "ipc/proc.h"
#include "util/logger.h"

namespace hcam {

namespace ipc {
void child_exit(int num) {
  hcam::logger::info(getpid(), "(child) exit");
  hcam::logger::stop_logger();
  exit(num);
}
} // namespace ipc
} // namespace hcam
