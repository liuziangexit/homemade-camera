#include "ipc/ipc.h"
#include "util/file_helper.h"
#include "util/logger.h"
#include "web/web.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

int quit = 0;

enum job_t { BAD_JOB = 99, CAPTURE = 0, NETWORK = 1, LOGGER = 2, END = 3 };
const char *names[] = {"CAPTURE", "NETWORK", "LOGGER "};
pid_t pids[END]{0};

//现在的一些逻辑是基于unix domain sock绝不会出错这一前提之下的...
//所有其他进程对ctl
int ctl_socks[END * 2]{0};
//所有其他进程对log
int log_socks[END * 2]{0};
// cap对net
int cap_net[2];
// net对ctl
int net_ctl[2];

job_t duty_by_pid(pid_t pid) {
  for (int i = 0; i < sizeof(pids) / sizeof(pid_t); i++) {
    if (pids[i] == pid)
      return (job_t)i;
  }
  return BAD_JOB;
}

void terminate_children() {
  hcam::logger::debug("terminating child processes...");
  for (int i = 0; i < sizeof(pids) / sizeof(pid_t); i++) {
    if (pids[i]) {
      hcam::logger::debug("sending EXIT to ", names[i], " ", pids[i]);
      if (hcam::ipc::send(ctl_socks[i * 2 + 1], "EXIT")) {
        hcam::logger::error("failed to kill child ", names[i], " ", pids[i]);
      }
      int status;
      hcam::logger::debug("waiting ", names[i], " ", pids[i]);
      int ret = waitpid(pids[i], &status, 0);
      if (ret == -1) {
        if (errno == ECHILD) {
          hcam::logger::warn("child ", names[i], " ", pids[i], " already dead");
          continue;
        }
        /*hcam::logger::fatal("waitpid failed");*/
        perror("waitpid failed");
        abort();
      }
      if (WIFEXITED(status)) {
        const int es = WEXITSTATUS(status);
        std::cout << "child " << names[i] << " " << pids[i]
                  << " exit status was " << es << "\r\n";
      } else if (WIFSIGNALED(status)) {
        const int es = WTERMSIG(status);
        std::cout << "child " << names[i] << " " << pids[i]
                  << " terminated by signal " << es << "\r\n";
      } else {
        //应该不会，如果这样了再看看怎么整
        std::cout << "unexpected waitpid return value"
                  << "\r\n";
        abort();
      }
    }
  }
}

void ctl_exit(int num) {
  hcam::logger::info("ctl_exit due to ", num);
  // unregister handler
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGCHLD, SIG_DFL);
  // kill children
  terminate_children();
  // close ipc socks
  //这个其实在这里关不关都不重要了，因为fork的时候把fd
  // table也复制了一份，所以每个
  // fd其实都是有一个引用计数的，我们在这里关了，也不过是引用计数-1。因为child
  // process没有close这些fd！
  //而在我们这代码里，是不可能让childprocess去close的，所以干脆不管了，反正又不在某些独特的嵌入式系统上跑，
  // 不closefd也不会泄漏资源什么的
  /*for (int i = 0; i < sizeof(ctl_socks) / sizeof(int); i++) {
    close(ctl_socks[i]);
  }
  for (int i = 0; i < sizeof(log_socks) / sizeof(int); i++) {
    close(log_socks[i]);
  }
  close(cap_net[0]);
  close(cap_net[1]);*/
  // hcam::logger::info( getpid(), "(ctl) exit");

  quit = 2;
  hcam::ipc::send(net_ctl[0], "EXIT");
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
      int es, quited = 0;
      if (WIFEXITED(status)) {
        es = WEXITSTATUS(status);
        quited = 1;
      } else if (WIFSIGNALED(status)) {
        es = WTERMSIG(status);
        quited = 1;
      }
      if (quited) {
        job_t d = duty_by_pid(pid);
        if (d == BAD_JOB) {
          hcam::logger::warn("duty_by_pid ", pid, " failed, quitting...");
          ctl_exit(251);
        }
        hcam::logger::warn("child ", names[d], " ", pids[d],
                           " exits unexpectedly, status was ", es);
        hcam::logger::warn("trying to respawn job ", names[d]);
        //为了避免拷贝此线程的logger context过去
        //先关掉logger，消掉logger context
        hcam::logger::stop_logger();
        pid_t child = born_child(d);
        hcam::logger::start_logger(log_socks[LOGGER * 2]);
        if (child == -1) {
          hcam::logger::warn("failed to create job ", names[d],
                             ", quitting...");
          ctl_exit(250);
        }
        int pong = ping_child(d);
        if (pong) {
          hcam::logger::warn("failed to respawn job ", names[d]);
          ctl_exit(244);
          abort();
        } else {
          hcam::logger::warn("job ", names[d], " ", child, " respawned");
        }
        pids[d] = child;
      }
    }
    break;
  case SIGINT:
  case SIGTERM:
    if (quit != 0) {
      return;
    }
    quit = 1;
    hcam::logger::info("signal ", signum, " received, quitting...");
    ctl_exit(signum);
  }
}

// child进程的
//不要让child被信号中止，而是parent收到信号之后，通过ipc通知child退出
void do_nothing_signal_handler(int signum) {}

extern void net_proc(int ctl_fd, int log_fd, int cap_fd, int net_ctl);
extern void cap_proc(int ctl_fd, int log_fd, int net_fd);
[[noreturn]] extern void logger_proc(int ctl_fd, int *client_fds,
                                     const char **client_names, int client_cnt);

void work(job_t duty) {
  //注册啥也不做的信号处理，这样就不会直接被信号杀掉，而是等ctl进程通知我们再死掉
  signal(SIGINT, do_nothing_signal_handler);
  signal(SIGTERM, do_nothing_signal_handler);
  switch (duty) {
  case CAPTURE:
    cap_proc(ctl_socks[duty * 2], log_socks[duty * 2], cap_net[1]);
    break;
  case NETWORK:
    net_proc(ctl_socks[duty * 2], log_socks[duty * 2], cap_net[0], net_ctl[0]);
    break;
  case LOGGER:
    int client_fds[END];
    for (int i = 0; i < END; i++) {
      client_fds[i] = log_socks[i * 2 + 1];
    }
    logger_proc(ctl_socks[duty * 2], client_fds, names, END);
    break;
  }
  hcam::logger::fatal("illegal job!");
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
    hcam::logger::fatal("should not reach here");
    abort();
  } else {
    return child;
  }
}

int ping_child(job_t j) {
  int sock = ctl_socks[j * 2 + 1];
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

void start_workers() {
  for (int i = 0; i < END; i++) {
    pid_t child = born_child((job_t)i);
    assert(child != 0);
    if (child == -1) {
      hcam::logger::info("failed to create job ", names[i], ", quitting...");
      ctl_exit(255);
    }
    pids[i] = child;
  }
}

bool verify_workers() {
  for (int i = 0; i < END; i++) {
    int ret = ping_child((job_t)i);
    if (ret) {
      hcam::logger::fatal("failed to contact child ", names[i]);
      return false;
    }
    hcam::logger::info("child ", names[i], " ", pids[i], " online");
  }
  return true;
}

int main(int argc, char **argv) {
  //准备ipc用的sock
  for (int i = 0; i < sizeof(ctl_socks) / sizeof(int) / 2; i++) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, &ctl_socks[i * 2])) {
      perror("socketpair failed");
      abort();
    }
  }
  for (int i = 0; i < sizeof(log_socks) / sizeof(int) / 2; i++) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, &log_socks[i * 2])) {
      perror("socketpair failed");
      abort();
    }
  }
  for (int i = 0; i < sizeof(cap_net) / sizeof(int) / 2; i++) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, &cap_net[i * 2])) {
      perror("socketpair failed");
      abort();
    }
  }
  for (int i = 0; i < sizeof(net_ctl) / sizeof(int) / 2; i++) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, &net_ctl[i * 2])) {
      perror("socketpair failed");
      abort();
    }
  }

  //创建工作进程
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  hcam::config::get();
  start_workers();

  //开始我自己的logger
  //用了LOGGER的位置，因为他们自己不需要这个log sock
  hcam::logger::start_logger(log_socks[LOGGER * 2]);

  //开始初始化control进程
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGCHLD, signal_handler);
  // 通过ipc验证刚创建的children是否还活着
  if (!verify_workers()) {
    ctl_exit(233);
  }

  // FIXME 卧槽，这就是UB吗？
  // std::this_thread::sleep_for(std::chrono::hours::max());

  /*while (quit != 2) {
    pause();
  }*/

  while (quit != 2) {
    auto message = hcam::ipc::recv(net_ctl[1]);
    if (message.first) {
      std::cout << "WHAT???main  hcam::ipc::recv failed!!";
      abort();
    }
    std::string text((char *)message.second.content, message.second.size);
    if (text == "EXIT")
      break;
    if (text == "RELOAD CONFIG") {
      hcam::logger::info("RELOAD CONFIG begin!!!");
      signal(SIGINT, SIG_IGN);
      signal(SIGTERM, SIG_IGN);
      signal(SIGCHLD, SIG_DFL);
      terminate_children();
      std::cout << "start_workers\r\n";
      //为了避免拷贝此线程的logger context过去
      //先关掉logger，消掉logger context
      bool new_config_ok = true;
      hcam::logger::stop_logger();

      hcam::config new_cfg;
      if (!new_cfg.read("config_new.json")) {
        new_config_ok = false;
      } else {
        std::vector<uint8_t> data;
        if (!hcam::read_file("config_new.json", data)) {
          std::cout << "hcam::read_file config_new.json failed!!!";
          abort();
        }
        if (!hcam::write_file("config.json", data.data(), data.size())) {
          std::cout << "hcam::write_file config_new.json failed!!!";
          abort();
        }
        remove("config_new.json");
        hcam::config::get_lazy().set_instance(new_cfg);
      }
      start_workers();
      hcam::logger::start_logger(log_socks[LOGGER * 2]);
      std::cout << "start_workers ok\r\n";
      signal(SIGINT, signal_handler);
      signal(SIGTERM, signal_handler);
      signal(SIGCHLD, signal_handler);
      std::cout << "verify_workers\r\n";
      if (!verify_workers()) {
        std::cout << "verify_workers failed\r\n";
        ctl_exit(254);
      }
      std::cout << "verify_workers ok\r\n";
      if (new_config_ok) {
        hcam::logger::info("RELOAD CONFIG ok!!!");
      } else {
        hcam::logger::info(
            "RELOAD CONFIG failed, currently using previous configuration");
      }
    }
  }
}
