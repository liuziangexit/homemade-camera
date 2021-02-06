#ifndef __HCAM_CAPTURE_H__
#define __HCAM_CAPTURE_H__

#include "codec.h"
#include "config/config.h"
#include "util/string_util.h"
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
#ifdef USE_V4L_CAPTURE
    std::shared_ptr<v4l_capture::buffer> captured_frame;
#endif
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
  int32_t diff = 0;
  //总处理帧数
  uint32_t frame_cnt = 0;

  static std::string make_filename(std::string save_directory,
                                   const std::string &file_format, time_t tm) {
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
    fmt << '.' << file_format;
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
