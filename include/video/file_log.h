#ifndef __HCAM_FILE_LOG_H__
#define __HCAM_FILE_LOG_H__
#include "json/json.hpp"
#include <deque>
#include <stdio.h>

namespace hcam {

struct file_log {
  using json = nlohmann::json;

  struct row {
    row(const std::string &_1, const std::string &_2, uint32_t _3, uint64_t _4,
        bool _5)
        : filename(_1), preview(_2), length(_3), time(_4), finished(_5) {}
    std::string to_str() const {
      return json{{"filename", filename},
                  {"preview", preview},
                  {"length", length},
                  {"time", time},
                  {"finished", finished}}
          .dump(4);
    }

    std::string filename;
    std::string preview;
    uint32_t length; //长度(秒)
    uint64_t time;   //开始时间
    bool finished;
  };

  std::deque<row> rows;

  bool parse(const std::string &str) {
    try {
      rows.clear();
      auto js = json::parse(str);
      for (const auto &r : js) {
        auto filename = r["filename"].get<std::string>();
        auto preview = r["preview"].get<std::string>();
        auto finished = r["finished"].get<bool>();
        auto length = r["length"].get<uint32_t>();
        auto time = r["time"].get<uint64_t>();
        rows.emplace_back(filename, preview, length, time, finished);
      }
    } catch (...) {
      return false;
    }
    return true;
  }

  void add(const std::string &filename, const std::string &preview,
           uint32_t length, uint64_t time, bool finished) {
    rows.emplace_back(filename, preview, length, time, finished);
  }

  row pop_front() {
    row copy = rows.front();
    rows.pop_front();
    return copy;
  }

  std::string to_str() {
    std::string str;
    str += "[";
    str += "\r\n";
    for (auto it = rows.begin(); it != rows.end(); it++) {
      str += it->to_str();
      if (it + 1 != rows.end()) {
        str += ",";
        str += "\r\n";
      }
    }
    str += "\r\n";
    str += "]";
    return str;
  }
};
} // namespace hcam

#endif