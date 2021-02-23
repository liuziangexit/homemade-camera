#include "ipc/ipc.h"
#include "util/logger.h"
#include "video/capture.h"
#include "web/web.h"
#include <iostream>
#include <opencv2/core/utility.hpp>
#include <signal.h>
#include <stdio.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/types.h> /* See NOTES */
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

hcam::capture *cap;
hcam::web *web;

int quit = 0;

enum job_t { CAPTURE = 0, NETWORK = 1, CONTROL = 2 };
job_t job;
pid_t children[CONTROL]{0};
int ipc_socks[2 * 3]{0};
int *ctl_cap = &ipc_socks[0], *ctl_net = &ipc_socks[2],
    *cap_net = &ipc_socks[4];

void hcam_exit(int num, bool is_ctl = false) {
  if (is_ctl) {
    // unregister handler
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    // kill children
    for (int i = 0; i < sizeof(children) / sizeof(pid_t); i++) {
      if (children[i]) {
        hcam::logger::warn("main", "killing ", children[i]);
        if (hcam::send_msg(ipc_socks[i * 2 + 1], "EXIT")) {
          hcam::logger::error("main", "failed to kill child ", i);
        }
        int status;
        int ret = waitpid(children[i], &status, 0);
        if (ret == -1) {
          hcam::logger::warn("main", "waitpid failed, what happened?");
          abort();
        }
        if (WIFEXITED(status)) {
          const int es = WEXITSTATUS(status);
          hcam::logger::info("main", "child ", children[i], " exit status was ",
                             es);
        } else {
          //应该不会，如果这样了再看看怎么整
          hcam::logger::fatal("main", "unexpected waitpid return value");
          abort();
        }
      }
    }
  }
  hcam::logger::info("main", getpid(), "(", (is_ctl ? "ctl" : "child"),
                     ") exit");
  exit(num);
}

void signal_handler(int signum) {
  switch (signum) {
  case SIGCHLD:
    pid_t pid;
    int status;
    //目前不存在process group，所以就不管waitpid()<-1的情况
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      if (WIFEXITED(status)) {
        const int es = WEXITSTATUS(status);
        hcam::logger::info("main", "child ", pid, " exit status was ", es);
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

    if (job == CAPTURE) {
      /*cap->stop();
      delete cap;*/
    } else if (job == NETWORK) {
      web->stop();
      delete web;
    }
    quit = true;
    quit = 2;

    hcam_exit(signum, job == CONTROL);
  }
}

//不要让child被信号中止，而是parent收到信号之后，通过ipc通知child退出
void do_nothing_siganl_handler(int signum) {}

int main(int argc, char **argv) {
  //准备ipc用的sock
  for (int i = 0; i < sizeof(ipc_socks) / sizeof(int) / 2; i++) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, &ipc_socks[i * 2])) {
      perror("socketpair failed");
      abort();
    }
  }

  //创建工作进程
  for (int i = 0; i < CONTROL; i++) {
    job = (job_t)i;
    pid_t child = fork();
    if (child == -1) {
      //创建失败
      hcam::logger::info("main", "failed to create job ", i, ", quitting...");
      hcam_exit(-1, true);
    }
    if (child == 0) {
      // child
      signal(SIGINT, do_nothing_siganl_handler);
      signal(SIGTERM, do_nothing_siganl_handler);
      goto WORK;
    }
    children[i] = child;
  }
  //开始初始化control进程
  job = CONTROL;
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGCHLD, signal_handler);
  hcam::logger::info("main", "CAPTURE:", children[CAPTURE],
                     " NETWORK:", children[NETWORK]);
  // 通过ipc验证刚创建的children是否还活着
  for (int i = 0; i < CONTROL; i++) {
    int sock = ipc_socks[i * 2 + 1];
    if (hcam::send_msg(sock, "PING")) {
      hcam::logger::error("main", "send_msg on child process ", children[i],
                          " failed");
      hcam_exit(-1, true);
    }
    if (hcam::wait_msg(sock, 2000) != 1) {
      hcam::logger::error("main", "hcam::wait_msg on child process ",
                          children[i], " failed");
      hcam_exit(-1, true);
    }
    auto response = hcam::recv_msg(sock);
    if (response.first) {
      hcam::logger::error("main", "hcam::recv_msg on child process ",
                          children[i], " failed");
      hcam_exit(-1, true);
    }
    std::string text((char *)response.second.content, response.second.size);
    if (text != "PONG") {
      hcam::logger::error("main", "child process ", children[i],
                          " responded with unexpected message");
      hcam_exit(-1, true);
    }
    hcam::logger::error("main", "child process ", children[i], " online");
  }

  //做工啦
WORK:
  switch (job) {
  case CAPTURE:
    cv::setNumThreads(hcam::config::get().video_thread_count);
    cap = new hcam::capture();
    cap->run(*ctl_cap);
    hcam_exit(0);
  case NETWORK:
    web = new hcam::web();
    web->run(*ctl_net);
    hcam_exit(0);
  case CONTROL:
    // FIXME 卧槽，这就是UB吗？
    // std::this_thread::sleep_for(std::chrono::hours::max());
    while (quit != 2) {
      pause();
    }
    break;
  }
  hcam::logger::fatal("main", "illegal job!");
  abort();
}