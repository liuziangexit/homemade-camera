#include "video/capture.h"
#include "config/config.h"
#include "omx/omx_jpg.h"
#include "util/guard.h"
#include "util/logger.h"
#include "util/string_util.h"
#include "util/time_util.h"
#include "video/codec.h"
#include "video/v4l_capture.h"
#include <algorithm>
#include <atomic>
#include <future>
#include <opencv2/core.hpp>
#include <opencv2/freetype.hpp>
#include <opencv2/videoio.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <time.h>
#include <utility>

// FIXME 用mutex

namespace hcam {

capture::capture(const config &_config) : m_config(_config) {}

capture::~capture() { stop(); }

void capture::run() {
  {
    uint32_t expect = 0;
    if (!m_flag.compare_exchange_strong(expect, 1))
      throw std::logic_error("capture already running");
  }
  std::thread(&capture::task, this, m_config).detach();
}

int capture::stop() {
  if (m_flag == 1) {
    m_flag = 2;
    while (m_flag != 3)
      ;
  }
  int r = (int)result;
  m_flag = 0;
  return r;
}

void capture::task(config config) {
  guard reset_flag([this]() {
    auto prev_state = m_flag.exchange(3);
    if (prev_state == 0 || prev_state == 3)
      throw std::runtime_error("capture flag is been tamper");
  });

  result = do_capture(config);
}

std::string capture::make_filename(std::string save_directory,
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

int capture::do_capture(const config &config) {
  if (config.duration < 1)
    return 4; //短于1秒的话文件名可能重复

  v4l_capture capture;
  if (capture.open(config.device,
                   v4l_capture::graphic{(uint32_t)config.resolution.width,
                                        (uint32_t)config.resolution.height,
                                        (uint32_t)config.fps,
                                        config.cam_pix_fmt})) {
    logger::error("VideoCapture open failed");
    return 1;
  }

  omx_jpg jpg_decoder;

  uint32_t fps = (uint32_t)config.fps;
  cv::Size frame_size(config.resolution);
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
  auto filename = make_filename(config.save_location,
                                codec_file_format(config.output_codec));
  if (!writer.open(filename, codec_fourcc(config.output_codec), fps,
                   frame_size)) {
    logger::error("VideoWriter open ", filename, " failed");
    return 2;
  }
  logger::info("video file change to ", filename);
  logger::info("capture backend:", "V4LCAPTURE",
               " writer backend:", writer.getBackendName(),
               " codec:", codec_to_string(config.output_codec), " fps:", fps,
               " resolution:", frame_size);

  uint32_t frame_cost = 0;

  while (true) {
    if (m_flag == 2) {
      break;
    }

    auto cap_ckp = checkpoint(3);
    std::pair<bool, std::shared_ptr<v4l_capture::buffer>> frame_jpg =
        capture.read();
    if (!frame_jpg.first) {
      logger::error("VideoCapture read failed");
      return 50;
    }
    auto decode_ckp = checkpoint(3);
    std::pair<bool, cv::Mat> decoded = jpg_decoder.decode(
        (unsigned char *)(frame_jpg.second->data), frame_jpg.second->length);
    if (!decoded.first) {
      logger::error("JPG decode failed");
      return 3;
    }
    cv::Mat &frame = decoded.second;

    auto draw_ckp = checkpoint(3);
    {
      //在指定位置渲染时间
      time_t tm;
      time(&tm);
      auto localt = localtime(&tm);
      std::ostringstream fmt(std::ios::app);
      fmt << localt->tm_year + 1900 << '/' << localt->tm_mon + 1 << '/'
          << localt->tm_mday << " " << localt->tm_hour << ':' << localt->tm_min
          << ':' << localt->tm_sec;

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

    auto write_ckp = checkpoint(3);
    writer.write(frame);
    auto done_ckp = checkpoint(3);
    if (done_ckp - cap_ckp > frame_time) {
      logger::warn("low frame rate, expect ", frame_time, "ms, actual ",
                   done_ckp - cap_ckp, //
                   "ms (capture:", decode_ckp - cap_ckp,
                   "ms, decode:", draw_ckp - decode_ckp,
                   "ms, draw:", write_ckp - draw_ckp,
                   "ms, write:", done_ckp - write_ckp, "ms)");
    } else {
      logger::info("cost ", done_ckp - cap_ckp, "ms");
    }
    frame_cost = done_ckp - cap_ckp;

    //到了预定的时间，换文件
    if (done_ckp - task_begin >= config.duration * 1000) {
      goto OPEN_WRITER;
    }
  }
  return 0;
}

void capture::render_text(int pos, const std::string &text, int font_height,
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
} // namespace hcam