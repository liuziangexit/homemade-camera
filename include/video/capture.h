#ifndef __HCAM_CAPTURE_H__
#define __HCAM_CAPTURE_H__
#include "codec.h"
#include "config/config.h"
#include "file_log.h"
#include "util/string_util.h"
#include "v4l_capture.h"
#include "web/web.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <opencv2/core.hpp>
#include <opencv2/freetype.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <optional>
#include <queue>
#include <stdio.h>
#include <string>
#include <thread>
#include <time.h>
#include <utility>
#include <vector>

#ifdef __linux__
#define USE_V4L_CAPTURE
#include "video/soft_jpg.h"
#include "video/v4l_capture.h"
#else
#include <opencv2/imgcodecs.hpp>
#endif

namespace hcam {

class capture {
public:
  capture(web &_web_service);
  void run();
  void stop();
  ~capture();

private:
  struct frame_context {
    frame_context() = default;
    frame_context(const frame_context &) = default;
    frame_context(frame_context &&) = default;

    //捕获帧的时间
    time_t frame_time;
    std::shared_ptr<v4l_capture::buffer> captured_frame;
    cv::Mat decoded_frame;
    //测性能
    uint32_t capture_time, send_time, send_done_time, decode_time,
        decode_done_time, process_time, write_time, done_time;
    //通知write线程退出
    bool quit = false;
  };

  enum { STOPPED, RUNNING, STOPPING };
  std::atomic<int> state = STOPPED;
  config _config;
  file_log log;
  std::atomic<bool> save_preview = false;
  std::string preview_filename;

  std::thread capture_thread;
  std::queue<frame_context> capture2decode_queue;
  std::mutex capture2decode_mtx;
  std::condition_variable capture2decode_cv;
  std::thread decode_thread;
  std::queue<frame_context> decode2write_queue;
  std::mutex decode2write_mtx;
  std::condition_variable decode2write_cv;
  std::thread write_thread;

  uint32_t pause_time;
  bool paused = false;
  std::mutex pause_mtx;
  std::condition_variable pause_cv;

  web &web_service;

  //帧速
  uint32_t frame_cost = 0;
  //总处理帧数
  uint32_t frame_cnt = 0;
  //帧率周期开始时的帧数
  uint32_t frame_cnt_base = 0;
  //帧率周期开始时间
  uint32_t base_time = 0;
  //此周期帧率
  uint32_t display_fps = 0;
  //此周期是否掉帧
  bool low_fps = false;

  static std::string read_log(std::string dir) {
    trim(dir);
    if (!dir.empty() && *dir.rbegin() != '/' && *dir.rbegin() != '\\')
      dir.append("/");
    dir += "file_log.json";
    std::unique_ptr<FILE, void (*)(FILE *)> fp(fopen(dir.c_str(), "rb"),
                                               [](FILE *fp) {
                                                 if (fp)
                                                   fclose(fp);
                                               });
    if (!fp) {
      return "[]";
    }
    fseek(fp.get(), 0L, SEEK_END);
    auto size = ftell(fp.get());
    if (size == -1L) {
      return "[]";
    }
    rewind(fp.get());
    std::string raw((std::size_t)size, 0);
    auto actual_read = fread(raw.data(), 1, size, fp.get());
    if (actual_read != size) {
      return "[]";
    }
    return raw;
  }

  static bool write_log(std::string dir, const std::string &content) {
    trim(dir);
    if (!dir.empty() && *dir.rbegin() != '/' && *dir.rbegin() != '\\')
      dir.append("/");
    dir += "file_log.json";
    std::unique_ptr<FILE, void (*)(FILE *)> fp(fopen(dir.c_str(), "wb"),
                                               [](FILE *fp) {
                                                 if (fp)
                                                   fclose(fp);
                                               });
    if (!fp)
      return false;
    auto actual_write = fwrite(content.data(), 1, content.size(), fp.get());
    if (actual_write != content.size())
      return false;
    return true;
  }

  static std::string make_filename(std::string save_directory, time_t tm) {
    trim(save_directory);
    if (!save_directory.empty() && *save_directory.rbegin() != '/' &&
        *save_directory.rbegin() != '\\')
      save_directory.append("/");

    //整一个文件名
    auto localt = localtime(&tm);
    std::ostringstream fmt(save_directory, std::ios_base::app);
    fmt << localt->tm_year + 1900 << '-' << localt->tm_mon + 1 << '-'
        << localt->tm_mday << " at " << localt->tm_hour << '.' << localt->tm_min
        << '.' << localt->tm_sec;
    return fmt.str();
  }

  static bool set_input_pixelformat(cv::VideoCapture &cap, codec c) {
    cap.set(cv::CAP_PROP_FOURCC, codec_fourcc(c));
    return codec_fourcc(c) == cap.get(cv::CAP_PROP_FOURCC);
  }

  void do_capture(const config &);
  void do_decode(const config &);
  void do_write(const config &);
  void render_text(int, const std::string &, int, std::optional<cv::Scalar>,
                   cv::freetype::FreeType2 *, cv::Mat &);
  void internal_stop_avoid_deadlock();
  bool pause_others();
  void resume_others();
  void wait_pause();
};

} // namespace hcam

#endif
