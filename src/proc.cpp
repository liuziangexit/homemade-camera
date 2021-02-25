//
// Created by 刘子昂 on 2021/2/23.
//

#include "ipc/proc.h"

namespace hcam {

namespace ipc {
void child_exit(int num) {
  hcam::logger::info("main", getpid(), "(child) exit");
  exit(num);
}
} // namespace ipc
} // namespace hcam
