#ifndef __HCAM_CAPTURE_H__
#define __HCAM_CAPTURE_H__

#include "codec.h"
#include "config/config.h"
#include "util/string_util.h"
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
#include <utility>
#include <vector>

namespace hcam {

class capture {
public:
  capture();

  void run();

  void stop();

  ~capture();

private:
  struct frame_context {
    frame_context() = default;
    frame_context(const frame_context &) = default;
    frame_context(frame_context &&) = default;

    cv::Mat frame;
    //测性能
    uint32_t capture_time, decode_time, decode_done_time, process_time,
        write_time, done_time;
    //通知write线程退出
    bool quit = false;
  };

  enum { STOPPED, RUNNING, STOPPING };
  std::atomic<int> state = STOPPED;
  config _config;
  // capture线程解码出来的帧，write线程会去这里拿
  std::queue<frame_context> frame_queue;
  std::mutex frames_mtx;
  std::condition_variable frames_cv;
  std::thread capture_thread, write_thread;
  //帧速
  uint32_t frame_cost = 0;

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

  void do_write(const config &);

  void render_text(int, const std::string &, int, std::optional<cv::Scalar>,
                   cv::freetype::FreeType2 *, cv::Mat &);

  void internal_stop_avoid_deadlock();
};

} // namespace hcam

#endif
