#include "util/logger.h"
#include "video/capture.h"
#include "web/web.h"
#include <iostream>
#include <opencv2/core/utility.hpp>
#include <signal.h>
#include <stdio.h>
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

void hcam_exit(int num, bool is_ctl = false) {
  if (is_ctl) {
    // unregister handler
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    // kill children
    for (int i = 0; i < sizeof(children) / sizeof(pid_t); i++) {
      if (children[i]) {
        //好像给parent发signal时候，也给child发了，所以就不用我们自己发了
        /*hcam::logger::warn("main", "killing ", children[i]);
        if (kill(children[i], SIGTERM)) {
          hcam::logger::error("main", "failed to kill child ", i);
        }*/
        int status;
        int ret = waitpid(children[i], &status, 0);
        if (ret == -1) {
          //可能是那个child正好在这之间的时候自己退出了，怎么会这么巧呢？但是是有可能的，是伐
          hcam::logger::warn("main", "waitpid failed?");
          continue;
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
    hcam_exit(signum, job == CONTROL);
  }
  /*if (quit != 0) {
    return;
  }
  quit = 1;
  hcam::logger::info("main", "signal ", signum, " received, quitting...");
  cap->stop();
  delete cap;
  web->stop();
  delete web;
  quit = true;
  quit = 2;
  exit(signum);*/
}

int ipc_socks[2 * 3]{0};
int *ctl_cap = &ipc_socks[0], *ctl_net = &ipc_socks[2],
    *cap_net = &ipc_socks[4];

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
      signal(SIGINT, signal_handler);
      signal(SIGTERM, signal_handler);
      goto WORK;
    }
    children[i] = child;
  }
  //开始初始化control进程
  job = CONTROL;
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGCHLD, signal_handler);
  // TODO 通过ipc验证刚创建的children是否还活着

  //做工啦
WORK:
  switch (job) {
  case CAPTURE:
    cv::setNumThreads(hcam::config::get().video_thread_count);
    hcam::logger::info("main", "capture job!");

    while (quit != 2) {
      pause();
    }

    hcam_exit(1);
    break;
  case NETWORK:
    hcam::logger::info("main", "network job!");

    while (quit != 2) {
      pause();
    }

    hcam_exit(2);
    break;
  case CONTROL:
    hcam::logger::info("main", "CAPTURE:", children[CAPTURE],
                       " NETWORK:", children[NETWORK]);
    // FIXME 卧槽，这就是UB吗？
    // std::this_thread::sleep_for(std::chrono::hours::max());
    while (quit != 2) {
      pause();
    }
    break;
  }
  hcam::logger::fatal("main", "illegal job!");
  abort();

  /*
    web = new hcam::web();
    web->run();

    cap = new hcam::capture(*web);
    cap->run();
*/
}