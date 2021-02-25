#include "ipc/ipc.h"
#include "util/logger.h"
#include "web/web.h"
#include <signal.h>
#include <stdio.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

int quit = 0;

enum job_t { BAD_JOB = 99, CAPTURE = 0, NETWORK = 1, END = 2 };
pid_t dead_children[END]{0};
pid_t children[END]{0};
int ipc_socks[2 * 3]{0};
int *ctl_cap = &ipc_socks[0], *ctl_net = &ipc_socks[2],
    *cap_net = &ipc_socks[4];

job_t duty_by_pid(pid_t pid) {
  for (int i = 0; i < sizeof(children) / sizeof(pid_t); i++) {
    if (children[i] == pid)
      return (job_t)i;
  }
  return BAD_JOB;
}

void ctl_exit(int num) {
  hcam::logger::info("main", "ctl_exit due to ", num);
  // unregister handler
  signal(SIGINT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);
  // kill children
  hcam::logger::debug("main", "terminating child processes...");
  for (int i = 0; i < sizeof(children) / sizeof(pid_t); i++) {
    if (children[i]) {
      hcam::logger::debug("main", "killing ", children[i]);
      if (hcam::ipc::send(ipc_socks[i * 2 + 1], "EXIT")) {
        hcam::logger::error("main", "failed to kill child ", i);
      }
      int status;
      int ret = waitpid(children[i], &status, 0);
      if (ret == -1) {
        if (errno == ECHILD) {
          hcam::logger::warn("main", "child ", i, " already dead");
          continue;
        }
        hcam::logger::fatal("main", "waitpid failed");
        perror("waitpid failed");
        abort();
      }
      if (WIFEXITED(status)) {
        const int es = WEXITSTATUS(status);
        hcam::logger::debug("main", "child ", children[i], " exit status was ",
                            es);
      } else {
        //应该不会，如果这样了再看看怎么整
        hcam::logger::fatal("main", "unexpected waitpid return value");
        abort();
      }
    }
  }
  // close ipc socks
  for (int i = 0; i < sizeof(ipc_socks) / sizeof(int); i++) {
    close(ipc_socks[i]);
  }
  hcam::logger::info("main", getpid(), "(ctl) exit");
  quit = 2;
  exit(num);
}

pid_t born_child(job_t duty);
int ping_child(job_t j);

// ctl进程的
void signal_handler(int signum) {
  switch (signum) {
  case SIGCHLD:
    pid_t pid;
    int status;
    //目前不存在process group，所以就不管waitpid()<-1的情况
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      //有个进程挂了！把它扶起来！在一个时间段内最多拉3次，如果还阿斗，那么就不扶了
      if (WIFEXITED(status)) {
        const int es = WEXITSTATUS(status);
        hcam::logger::warn("main", "child ", pid,
                           " exits unexpectedly, status was ", es);
        job_t d = duty_by_pid(pid);
        if (d == BAD_JOB) {
          hcam::logger::warn("main", "duty_by_pid failed, quitting...");
          ctl_exit(251);
        }
        if (dead_children[d]) {
          // failed to respawned job, ignore it
          continue;
        }
        hcam::logger::warn("main", "trying to respawn job ", d);
        pid_t child = born_child(d);
        if (child == -1) {
          hcam::logger::warn("main", "failed to create job ", d,
                             ", quitting...");
          ctl_exit(250);
        }
        int pong = ping_child(d);
        if (pong) {
          hcam::logger::warn("main", "failed to respawn job ", d,
                             ", keep running without it...");
          dead_children[d] = 1;
          /*ctl_exit(244);
          abort();*/
        } else {
          hcam::logger::warn("main", "job ", d, " respawned");
        }
        children[d] = child;
      }
    }
    break;
  case SIGINT:
  case SIGTERM:
    if (quit != 0) {
      return;
    }
    quit = 1;
    hcam::logger::info("main", "signal ", signum, " received, quitting...");
    ctl_exit(signum);
  }
}

// child进程的
//不要让child被信号中止，而是parent收到信号之后，通过ipc通知child退出
void do_nothing_signal_handler(int signum) {}

extern void net_proc(int ctl_fd, int cap_fd);
extern void cap_proc(int ctl_fd, int net_fd);

void work(job_t duty) {
  //注册啥也不做的信号处理，这样就不会直接被信号杀掉，而是等ctl进程通知我们再死掉
  signal(SIGINT, do_nothing_signal_handler);
  signal(SIGTERM, do_nothing_signal_handler);
  switch (duty) {
  case CAPTURE:
    net_proc(ctl_cap[0], cap_net[0]);
    break;
  case NETWORK:
    cap_proc(ctl_net[0], cap_net[1]);
    break;
  }
  hcam::logger::fatal("main", "illegal job!");
  abort();
}

pid_t born_child(job_t duty) {
  pid_t child = fork();
  if (child == -1) {
    return -1;
  }
  if (child == 0) {
    // child
    work(duty);
    // never reach
    hcam::logger::fatal("main", "should not reach here");
    abort();
  } else {
    return child;
  }
}

int ping_child(job_t j) {
  int sock = ipc_socks[j * 2 + 1];
  if (hcam::ipc::send(sock, "PING")) {
    return 1;
  }
  if (hcam::ipc::wait(sock, 2000) != 1) {
    return 2;
  }
  auto response = hcam::ipc::recv(sock);
  if (response.first) {
    return 3;
  }
  std::string text((char *)response.second.content, response.second.size);
  if (text != "PONG") {
    return 4;
  }
  return 0;
}

// TODO 显示proc的名字，还要设置proc的名字
void proc_name() {}

int main(int argc, char **argv) {
  //准备ipc用的sock
  for (int i = 0; i < sizeof(ipc_socks) / sizeof(int) / 2; i++) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, &ipc_socks[i * 2])) {
      perror("socketpair failed");
      abort();
    }
  }

  //创建工作进程
  for (int i = 0; i < END; i++) {
    pid_t child = born_child((job_t)i);
    assert(child != 0);
    if (child == -1) {
      hcam::logger::info("main", "failed to create job ", i, ", quitting...");
      ctl_exit(255);
    }
    children[i] = child;
  }

  //开始初始化control进程
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGCHLD, signal_handler);
  // 通过ipc验证刚创建的children是否还活着
  for (int i = 0; i < END; i++) {
    int ret = ping_child((job_t)i);
    if (ret) {
      hcam::logger::fatal("main", "failed to contact child ", i);
      ctl_exit(233);
    }
    hcam::logger::info("main", "child ", i, " online");
  }

  // FIXME 卧槽，这就是UB吗？
  // std::this_thread::sleep_for(std::chrono::hours::max());
  while (quit != 2) {
    pause();
  }
}