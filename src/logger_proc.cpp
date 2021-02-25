#include "config/config.h"
#include "ipc/ipc.h"
#include "ipc/proc.h"
#include <iostream>
#include <limits>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <utility>
#include <vector>
using namespace hcam;

int max(int *arr, int cnt) {
  int m = INT_MIN;
  for (int i = 0; i < cnt; i++) {
    if (arr[i] > m)
      m = arr[i];
  }
  return m;
}

std::pair<bool, std::string> process_message(int level,
                                             const std::string &module,
                                             time_t time,
                                             const std::string &message) {
  if (level < config::get().log_level) {
    return std::pair<bool, std::string>(false, std::string());
  }
  const auto &set = config::get().disable_log_module;
  if (set.find(module) != set.end()) {
    return std::pair<bool, std::string>(false, std::string());
  }

  std::ostringstream fmt;
  if (level == 0) {
    fmt << "[debg] ";
  } else if (level == 1) {
    fmt << "[info] ";
  } else if (level == 2) {
    fmt << "[warn] ";
  } else if (level == 3) {
    fmt << "[erro] ";
  } else if (level == 4) {
    fmt << "[fatl] ";
  } else {
    throw std::invalid_argument("");
  }
  fmt << module << " at ";
  auto localt = localtime(&time);
  fmt << localt->tm_year + 1900 << '/' << std::setfill('0') << std::setw(2)
      << localt->tm_mon + 1 << '/' << std::setfill('0') << std::setw(2)
      << localt->tm_mday << ' ' << std::setfill('0') << std::setw(2)
      << localt->tm_hour << ':' << std::setfill('0') << std::setw(2)
      << localt->tm_min << ':' << std::setfill('0') << std::setw(2)
      << localt->tm_sec;

  fmt << ": " << message << "\r\n";

  return std::pair<bool, std::string>(true, fmt.str());
}

[[noreturn]] void logger_proc(int ctl_fd, int *client_fds,
                              const char **client_names, int client_cnt) {
  std::unique_ptr<FILE, void (*)(FILE *)> fp(
      fopen(config::get().log_file.c_str(),
            config::get().log_fopen_mode.c_str()),
      [](FILE *p) { fclose(p); });
  if (!fp) {
    std::cout << "unable to open log file, logger process quitting...";
    ipc::child_exit(1);
  }
  while (true) {
    int ret;
    fd_set fds;

    int fuck = max(client_fds, client_cnt);
    if (ctl_fd > fuck)
      fuck = ctl_fd;
    fuck++;

  SELECT:
    FD_ZERO(&fds);
    FD_SET(ctl_fd, &fds);
    for (int i = 0; i < client_cnt; i++) {
      FD_SET(client_fds[i], &fds);
    }
    ret = select(fuck, &fds, NULL, NULL, NULL);
    if (ret == -1) {
      if (EINTR == errno)
        goto SELECT;
      logger::info("select failed, quitting...");
      ipc::child_exit(77);
    }

    //处理与ctl沟通的sock
    if (FD_ISSET(ctl_fd, &fds)) {
      auto msg = ipc::recv(ctl_fd);
      if (msg.first) {
        logger::error("ipc handler read failed, quitting...", msg.first);
        ipc::child_exit(78);
      }
      std::string text((char *)msg.second.content, msg.second.size);
      if (text == "PING") {
        //心跳
        if (ipc::send(ctl_fd, "PONG")) {
          // something goes wrong
          ipc::child_exit(88);
        }
      } else if (text == "EXIT") {
        logger::debug("IPC EXIT, quitting...");
        ipc::child_exit(79);
      }
    } else {
      //处理client
      std::vector<std::pair<uint64_t, std::string>> logs;
      for (int i = 0; i < client_cnt; i++) {
        if (FD_ISSET(client_fds[i], &fds)) {
          int client = client_fds[i];

          uint64_t send_time;
          auto send_time_raw = ipc::recv(client);
          if (send_time_raw.first)
            ipc::child_exit(91);
          if (send_time_raw.second.size != sizeof(send_time))
            ipc::child_exit(92);
          memcpy(&send_time, send_time_raw.second.content,
                 send_time_raw.second.size);

          uint32_t level;
          auto level_raw = ipc::recv(client);
          if (level_raw.first)
            ipc::child_exit(81);
          if (level_raw.second.size != sizeof(level))
            ipc::child_exit(82);
          memcpy(&level, level_raw.second.content, level_raw.second.size);

          time_t time;
          auto time_raw = ipc::recv(client);
          if (time_raw.first)
            ipc::child_exit(83);
          if (time_raw.second.size != sizeof(time))
            ipc::child_exit(84);
          memcpy(&time, time_raw.second.content, time_raw.second.size);

          auto message_raw = ipc::recv(client);
          if (message_raw.first)
            ipc::child_exit(85);
          std::string message((char *)message_raw.second.content,
                              message_raw.second.size);

          //这里我还做了个排序，但这个其实仔细想想还是不能解决log有时候乱序的问题的
          // FIXME 看看怎么解决log有可能乱序这个问题
          auto fmt = process_message(level, client_names[i], time, message);
          if (fmt.first) {
            bool pushed = false;
            for (auto it = logs.begin(); it != logs.end(); it++) {
              if (send_time < it->first) {
                logs.emplace(it, send_time, std::move(fmt.second));
                pushed = true;
                break;
              }
            }
            if (!pushed) {
              logs.emplace_back(send_time, std::move(fmt.second));
            }
          }

          /*if (!ipc::send(client, "OK"))
            ipc::child_exit(86);*/
        }
      }
      for (const auto &log : logs) {
        std::cout << log.second;
        if (fwrite(log.second.data(), 1, log.second.size(), fp.get()) !=
            log.second.size()) {
          std::cout << "unable to write log file, logger process quitting...";
          ipc::child_exit(2);
        }
        if (fflush(fp.get())) {
          std::cout << "unable to flush log file, logger process quitting...";
          ipc::child_exit(3);
        }
      }
    }
  }
}
