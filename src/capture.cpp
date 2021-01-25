#include "video/capture.h"
#include "config/config.h"
#include "util/logger.h"
#include "util/time_util.h"
#include "video/codec.h"
#include "video/soft_jpg.h"
#include "video/v4l_capture.h"
#include <algorithm>
#include <atomic>
#include <future>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/freetype.hpp>
#include <opencv2/videoio.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <time.h>
#include <utility>

#ifdef __linux__
#define USE_V4L_CAPTURE
#endif

namespace hcam {

capture::capture(const config &_config) : _config(_config) {}

capture::~capture() { stop(); }

void capture::run() {
  {
    int expect = STOPPED;
    if (!state.compare_exchange_strong(expect, RUNNING))
      throw std::logic_error("capture already running");
  }
  capture_thread = std::thread([this] { this->do_capture(this->_config); });
  write_thread = std::thread([this] { this->do_write(this->_config); });
}

void capture::stop() {
  {
    int expect = RUNNING;
    if (!state.compare_exchange_strong(expect, STOPPING)) {
      logger::info("stop failed, expect state ", RUNNING, ", actual state ",
                   expect);
      return;
    }
  }
  if (std::this_thread::get_id() != capture_thread.get_id()) {
    if (capture_thread.joinable())
      capture_thread.join();
  }
  if (std::this_thread::get_id() != write_thread.get_id()) {
    if (write_thread.joinable())
      write_thread.join();
  }
  state = STOPPED;
}

void capture::internal_stop_avoid_deadlock() {
  std::thread([this] { stop(); }).detach();
}

void capture::do_capture(const config &config) {
#ifdef USE_V4L_CAPTURE
  v4l_capture capture;
  if (capture.open(config.device,
                   v4l_capture::graphic{(uint32_t)config.resolution.width,
                                        (uint32_t)config.resolution.height,
                                        (uint32_t)config.fps,
                                        config.cam_pix_fmt})) {
    logger::error("VideoCapture open failed");
    return;
  }
  soft_jpg jpg_decoder;

  auto get_frame = [](frame_context &ctx) {
    ctx.capture_time = checkpoint(3);
    std::pair<bool, std::shared_ptr<v4l_capture::buffer>> packet =
        capture.read();
    if (!packet.first) {
      logger::error("VideoCapture read failed");
      return false;
    }
    ctx.decode_time = checkpoint(3);
    std::pair<bool, cv::Mat> decoded = jpg_decoder.decode(
        (unsigned char *)(packet.second->data), packet.second->length);
    if (!decoded.first) {
      logger::error("JPG decode failed");
      return false;
    }
    ctx.frame = std::move(decoded.second);
    ctx.decode_done_time = checkpoint(3);
  };
#else
  cv::VideoCapture capture;
  if (config.device.empty()) {
    if (!capture.open(0))
      throw std::runtime_error("capture open failed");
  } else {
    if (!capture.open(config.device))
      throw std::runtime_error("capture open failed");
  }

  auto get_frame = [&capture](frame_context &ctx) {
    ctx.capture_time = checkpoint(3);
    cv::Mat mat;
    if (!capture.read(mat)) {
      logger::error("!VideoCapture read failed");
      return false;
    }
    ctx.decode_time = checkpoint(3);
    ctx.decode_done_time = ctx.decode_time;
    ctx.frame = std::move(mat);
    return true;
  };
#endif

  while (true) {
    if (state == STOPPING) {
      break;
    }

    frame_context ctx;
    if (!get_frame(ctx)) {
      break;
    }

    std::unique_lock l(frames_mtx);
    frame_queue.push(std::move(ctx));
    if (frame_queue.size() > 10) {
      //消费跟不上生产
      logger::warn("frame_queue.size(): ", frame_queue.size());
    }
    frames_cv.notify_one();
  }
  std::unique_lock l(frames_mtx);
  frame_context ctx;
  ctx.quit = true;
  frame_queue.push(ctx);
  frames_cv.notify_one();
}

void capture::do_write(const config &config) {
  static const int open_writer_retry = 3;

  if (config.duration < 1)
    this->stop(); //短于1秒的话文件名可能重复

  auto fps = (uint32_t)config.fps;
  cv::Size frame_size(config.resolution);
  const uint64_t expect_frame_time = 1000 / fps;

  cv::Ptr<cv::freetype::FreeType2> freetype = cv::freetype::createFreeType2();
  if (freetype.empty()) {
    logger::error("create freetype2 instance failed");
    this->stop();
  }
  freetype->loadFontData("Helvetica.ttc", 0);

  //不知道为什么，在idea里面调试的时候，如果触发SIGINT，writer们从来不能正常destruct，先不管
  std::thread prepare_writer_thread, destruct_thread;
  std::unique_ptr<cv::VideoWriter> writer, next_writer;
  std::string writer_filename, next_writer_filename;
  std::atomic<bool> next_writer_mut;
  bool first_time = true;
  auto prepare_writer = [config, frame_size,
                         fps](time_t date,
                              std::unique_ptr<cv::VideoWriter> &writer,
                              std::string &filename) {
    filename = make_filename(config.save_location,
                             codec_file_format(config.output_codec), date);
    writer = std::make_unique<cv::VideoWriter>();

    for (int tried = 0; tried < open_writer_retry; tried++) {
      if (writer->open(filename, codec_fourcc(config.output_codec), fps,
                       frame_size)) {
        return true;
      } else {
        logger::error("VideoWriter open ", filename, " failed ", tried + 1);
      }
    }
    return false;
  };

OPEN_WRITER:
  if (first_time) {
    // first time!
    time_t tm;
    time(&tm);
    if (!prepare_writer(tm, writer, writer_filename)) {
      logger::error("prepare_writer failed");
      this->stop();
    }
    first_time = false;
  } else {
    {
      bool expect = false;
      if (!next_writer_mut.compare_exchange_strong(expect, true)) {
        logger::error("next_writer_mut.compare_exchange_strong failed");
        this->stop();
      }
    }
    writer = std::move(next_writer);
    writer_filename = std::move(next_writer_filename);
    next_writer_mut.store(false);
  }

  // prepare next writer asynchronous
  {
    time_t tm;
    time(&tm);
    tm += config.duration;
    if (prepare_writer_thread.joinable())
      prepare_writer_thread.join();
    prepare_writer_thread =
        std::thread([this, tm, &next_writer, &next_writer_mut, prepare_writer,
                     &next_writer_filename] {
          {
            bool expect = false;
            if (!next_writer_mut.compare_exchange_strong(expect, true)) {
              {
                logger::error("next_writer_mut.compare_exchange_strong failed");
                internal_stop_avoid_deadlock();
              }
            }
          }
          if (!prepare_writer(tm, next_writer, next_writer_filename)) {
            logger::error("prepare_writer failed");
            internal_stop_avoid_deadlock();
          }
          next_writer_mut.store(false);
        });
  }

  auto task_begin = checkpoint(3);
  logger::info("video file change to ", writer_filename);
  logger::info(" writer backend:", writer->getBackendName(),
               " codec:", codec_to_string(config.output_codec), " fps:", fps,
               " resolution:", frame_size);

  while (true) {
    if (state == STOPPING) {
      break;
    }

    std::unique_lock l(frames_mtx);
    frames_cv.wait(l, [this] { return !frame_queue.empty(); });
    frame_context ctx = std::move(frame_queue.front());
    frame_queue.pop();
    if (ctx.quit) {
      break;
    }
    l.unlock();

    cv::Mat &frame = ctx.frame;

    ctx.process_time = checkpoint(3);
    {
      //在指定位置渲染时间
      time_t tm;
      time(&tm);
      auto localt = localtime(&tm);
      std::ostringstream fmt(std::ios::app);
      fmt << localt->tm_year + 1900 << '/' << localt->tm_mon + 1 << '/'
          << localt->tm_mday << " " << localt->tm_hour << ':' << localt->tm_min
          << ':' << localt->tm_sec;

      render_text(config.timestamp_pos, fmt.str(), config.font_height,
                  std::optional<cv::Scalar>(), freetype, frame);
      //渲染帧率
      if (frame_cost != 0) {
        auto low_fps = frame_cost > expect_frame_time;
        if (low_fps) {
          if (config.display_fps == 1 || config.display_fps == 2) {
            fmt.str("LOW FPS: ");
          }
        } else {
          if (config.display_fps == 2) {
            fmt.str("FPS: ");
          }
        }
        fmt << 1000 / frame_cost;
        render_text((config.timestamp_pos + 1) % 4, fmt.str(),
                    config.font_height,
                    low_fps ? cv::Scalar((double)30, (double)120, (double)238)
                            : std::optional<cv::Scalar>(),
                    freetype, frame);
      }
    }

    ctx.write_time = checkpoint(3);
    writer->write(frame);
    ctx.done_time = checkpoint(3);

    frame_cost = ctx.decode_done_time - ctx.capture_time;
    if (ctx.done_time - ctx.process_time > frame_cost) {
      frame_cost = ctx.done_time - ctx.process_time;
    }

    if (frame_cost > expect_frame_time) {
      logger::warn("low frame rate, expect ", expect_frame_time, "ms, actual ",
                   frame_cost, //
                   "ms (capture:", ctx.decode_time - ctx.capture_time,
                   "ms, decode:", ctx.decode_done_time - ctx.decode_time,
                   "ms, inter-thread:", ctx.process_time - ctx.decode_done_time,
                   "ms, process:", ctx.write_time - ctx.process_time,
                   "ms, write:", ctx.done_time - ctx.write_time, "ms)");
    } else {
      logger::info("cost ", frame_cost, "ms");
    }

    //到了预定的时间，换文件
    // FIXME linux的mono时间是不会往回的吧？确认一下
    if (ctx.done_time - task_begin >= config.duration * 1000) {
      auto writer_p = writer.release();
      if (destruct_thread.joinable())
        destruct_thread.join();
      destruct_thread = std::thread([writer_p] {
        // destruct时候会开始做一些文件的写入工作，如果文件很大，会花上许多秒或者更久，那么工作队列里的东西就开始堆积了
        // 所以我们在另一个线程做destruct
        delete writer_p;
      });
      goto OPEN_WRITER;
    }
  }
  if (destruct_thread.joinable())
    destruct_thread.join();
  if (prepare_writer_thread.joinable())
    prepare_writer_thread.join();
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