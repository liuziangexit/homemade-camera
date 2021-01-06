#ifndef __HOMEMADECAM_CAPTURE_H__
#define __HOMEMADECAM_CAPTURE_H__

#include "codec.h"
#include "config/config.h"
#include "util/guard.h"
#include "util/logger.h"
#include "util/string_util.h"
#include "util/time_util.h"
#include <algorithm>
#include <atomic>
#include <future>
#include <opencv2/core.hpp>
#include <opencv2/freetype.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/time.h>
#include <thread>
#include <time.h>
#include <utility>
#include <vector>

// FIXME 结束等待时候不要spin

namespace homemadecam {

class capture {
public:
  capture(const config &_config) : config(_config) {}

  void run() {
    {
      uint32_t expect = 0;
      if (!flag.compare_exchange_strong(expect, 1))
        throw std::logic_error("capture already running");
    }
    std::thread(&capture::task, this, config).detach();
  }

  volatile int result = 0;

  int stop() {
    if (flag == 1) {
      flag = 2;
      while (flag != 3)
        ;
    }
    int r = (int)result;
    flag = 0;
    return r;
  }

  ~capture() { stop(); }

private:
  // 0-未开始,1-开始,2-要求结束,3-正在结束
  std::atomic<uint32_t> flag = 0;

  void task(config config) {
    guard reset_flag([this]() {
      auto prev_state = flag.exchange(3);
      if (prev_state == 0 || prev_state == 3)
        throw std::runtime_error("capture flag is been tamper");
    });

    result = do_capture(config);
  }

  std::string make_filename(std::string save_directory,
                            const std::string &file_format) {
    trim(save_directory);
    if (!save_directory.empty() && *save_directory.rbegin() != '/' &&
        *save_directory.rbegin() != '\\')
      save_directory.append("/");

    //整一个文件名
    time_t tm;
    time(&tm);
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

  static bool set_resolution(cv::VideoCapture &cap, cv::Size res) {
    cap.set(cv::CAP_PROP_FRAME_WIDTH, res.width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, res.height);
    return cap.get(cv::CAP_PROP_FRAME_WIDTH) == res.width &&
           cap.get(cv::CAP_PROP_FRAME_HEIGHT) == res.height;
  }

  static bool set_fps(cv::VideoCapture &cap, int fps) {
    cap.set(cv::CAP_PROP_FPS, fps);
    return cap.get(cv::CAP_PROP_FPS) == fps;
  }

  int do_capture(config &config) {
    if (config.duration < 1)
      return 4; //短于1秒的话文件名可能重复
    cv::VideoCapture capture;
#ifdef __APPLE__
#define API cv::CAP_AVFOUNDATION
#elif __linux__
#define API cv::CAP_V4L
#else
#define API cv::CAP_ANY
#endif

    if (!capture.open(config.camera_id, API)) {
      logger::error("VideoCapture open failed");
      return 1;
    }

    if (!set_resolution(capture, config.resolution)) {
      logger::error("VideoCapture set resolution failed");
      return 63;
    }

    double fps = (int)capture.get(cv::CAP_PROP_FPS);
    cv::Size frame_size(capture.get(cv::CAP_PROP_FRAME_WIDTH),
                        capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    const uint64_t frame_time = 1000 / fps;

    cv::Ptr<cv::freetype::FreeType2> freetype = cv::freetype::createFreeType2();
    if (freetype.empty()) {
      logger::error("create freetype2 instance failed");
      return 5;
    }

    freetype->loadFontData("Helvetica.ttc", 0);
    cv::VideoWriter writer;

  OPEN_WRITER:
    auto task_begin = checkpoint(3);
    auto filename =
        make_filename(config.save_location, codec_file_format(config.codec));
    if (!writer.open(filename, codec_fourcc(config.codec), fps, frame_size)) {
      logger::error("VideoWriter open ", filename, " failed");
      return 2;
    }
    logger::info("video file change to ", filename);
    logger::info("capture backend:", capture.getBackendName(),
                 " writer backend:", writer.getBackendName(), " fps:", fps,
                 " resolution:", frame_size);

    uint32_t frame_cost = 0;

    while (true) {
      if (flag == 2) {
        break;
      }

      auto frame_begin = checkpoint(3);
      cv::Mat frame;
      if (!capture.grab()) {
        logger::error("VideoCapture grab failed");
        continue;
      }
      auto frame_grab = checkpoint(3);
      if (!capture.retrieve(frame)) {
        logger::error("VideoCapture retrieve failed");
        return 3;
      }
      auto frame_retrieve = checkpoint(3);

      {
        //在指定位置渲染时间
        time_t tm;
        time(&tm);
        auto localt = localtime(&tm);
        std::ostringstream fmt(std::ios::app);
        fmt << localt->tm_year + 1900 << '/' << localt->tm_mon + 1 << '/'
            << localt->tm_mday << " " << localt->tm_hour << ':'
            << localt->tm_min << ':' << localt->tm_sec;

        render_text(config.text_pos, fmt.str(), config.font_height,
                    std::optional<cv::Scalar>(), freetype, frame);
        //低帧率时渲染警告
        if (frame_cost > frame_time) {
          fmt.str("LOW FPS: ");
          fmt << 1000 / frame_cost;
          render_text((config.text_pos + 1) % 4, fmt.str(), config.font_height,
                      std::make_optional(
                          cv::Scalar((double)30, (double)120, (double)238)),
                      freetype, frame);
        }
      }

      auto frame_drawtext = checkpoint(3);
      writer.write(frame);
      auto frame_write = checkpoint(3);
      if (frame_write - frame_begin > frame_time) {
        logger::warn("low frame rate, expect ", frame_time, "ms, actual ",
                     frame_write - frame_begin,
                     "ms(grab:", frame_grab - frame_begin,
                     "ms, retrieve:", frame_retrieve - frame_grab,
                     "ms, drawtext:", frame_drawtext - frame_retrieve,
                     "ms, write:", frame_write - frame_drawtext, "ms)");
      } else {
        logger::info("cost ", frame_write - frame_begin, "ms");
      }
      frame_cost = frame_write - frame_begin;

      //到了预定的时间，换文件
      if (frame_write - task_begin >= config.duration * 1000) {
        goto OPEN_WRITER;
      }
    }
    return 0;
  }

  void render_text(int pos, const std::string &text, int font_height,
                   std::optional<cv::Scalar> foreground,
                   cv::freetype::FreeType2 *freetype, cv::Mat &img) {
    int channel = img.channels();
    int depth = img.depth();
    if (channel != 3 || depth != CV_8U)
      throw std::runtime_error("render_text: channel != 3 || depth != 8");

    int baseline;
    const int thickness = -1;
    //计算渲染文字的位置
    cv::Size text_render_size =
        freetype->getTextSize(text, font_height, thickness, &baseline);
    cv::Point text_render_pos;
    if (pos == 0) {
      //右上
      text_render_pos = {img.cols - text_render_size.width, 0};
    } else if (pos == 1) {
      //左上
      text_render_pos = {0, 0};
    } else if (pos == 2) {
      //左下
      text_render_pos = {0, img.rows - text_render_size.height};
    } else if (pos == 3) {
      //右下
      text_render_pos = {img.cols - text_render_size.width,
                         img.rows - text_render_size.height};
    } else if (pos == 4) {
      //中间
      text_render_pos = {(img.cols - text_render_size.width) / 2,
                         (img.rows - text_render_size.height) / 2};
    }
    cv::Scalar color{0, 0, 0};
    if (foreground) {
      color = *foreground;
    } else {
      //计算该区域内的平均颜色，然后决定渲染的时候使用黑或白
      const std::size_t xend =
          std::min(img.cols, text_render_pos.x + text_render_size.width);
      const std::size_t yend =
          std::min(img.rows, text_render_pos.y + text_render_size.height);
      const std::size_t pixel_count =
          (xend - text_render_pos.x) * (yend - text_render_pos.y);
      //计算平均颜色
      auto color_channel_sum = [&color]() -> uint64_t {
        return color[0] + color[1] + color[2];
      };
      for (std::size_t x = text_render_pos.x; x < xend; x++) {
        for (std::size_t y = text_render_pos.y; y < yend; y++) {
          color[2] +=
              ((uint8_t *)
                   img.data)[y * img.cols * channel + x * channel + 2]; // R
          color[1] +=
              ((uint8_t *)
                   img.data)[y * img.cols * channel + x * channel + 1]; // G
          color[0] +=
              ((uint8_t *)
                   img.data)[y * img.cols * channel + x * channel + 0]; // B
        }
      }
      color[0] /= pixel_count;
      color[1] /= pixel_count;
      color[2] /= pixel_count;
      auto avg_sum = (int64_t)color_channel_sum();
      //反色
      color[0] = 255 - color[0];
      color[1] = 255 - color[1];
      color[2] = 255 - color[2];
      auto rev_sum = (int64_t)color_channel_sum();
      //如果平均色和反色相差不够大，就用黑色或白色
      if (abs(avg_sum - rev_sum) < 255 * 3 / 2) {
        if (avg_sum >= 255 * 3 / 2) {
          color = {0, 0, 0};
        } else {
          color = {255, 255, 255};
        }
      }
    }

    // FIXME 这肯定是我们的一个bug，opencv不可能有错啊！
    //为啥要-height/2？我不懂啊
    if (pos > 1)
      text_render_pos += cv::Point(0, -text_render_size.height / 2);
    freetype->putText(img, text, text_render_pos, font_height, color, thickness,
                      8, false);
  }

  typename homemadecam::config config;
};

} // namespace homemadecam

#endif
