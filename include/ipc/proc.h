//
// Created by 刘子昂 on 2021/2/23.
//

#ifndef HOMECAM_PROC_H
#define HOMECAM_PROC_H
#include <cstdint>
#include <memory>
#include <stdlib.h>
#include <sys/socket.h>
#include <utility>

namespace hcam {
namespace ipc {
void child_exit(int num);
} // namespace ipc
} // namespace hcam

#endif // HOMECAM_IPC_H
