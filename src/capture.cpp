#include "video/capture.h"
#include "config/config.h"
#include "util/logger.h"
#include "util/time_util.h"
#include "video/codec.h"
#include "video/hard_jpg.h"
#include "video/soft_jpg.h"
#include "web/session.h"
#include <algorithm>
#include <atomic>
#include <future>
#include <math.h>
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

namespace hcam {

capture::capture(web &_web_service)
    : _config(config::get()), web_service(_web_service) {}
capture::~capture() { stop(); }

void capture::run() {
  {
    int expect = STOPPED;
    if (!state.compare_exchange_strong(expect, RUNNING))
      throw std::logic_error("capture already running");
  }
  auto json_string = read_log(_config.save_location);
  log.parse(json_string);

  worker_thread = std::thread([this] { this->do_work(this->_config); });
}

void capture::stop(bool join) {
  logger::debug("cap", "stopping capture");
  {
    int expect = RUNNING;
    if (!state.compare_exchange_strong(expect, STOPPING)) {
      logger::debug("cap", "stop failed, expect state ", RUNNING,
                    ", actual state ", expect);
      return;
    }
  }
  if (join) {
    if (worker_thread.joinable())
      worker_thread.join();
  }
  state = STOPPED;
  logger::info("cap", "capture stopped");
}

void capture::do_work(const config &config) {
  // prepare freetype
  cv::Ptr<cv::freetype::FreeType2> freetype = cv::freetype::createFreeType2();
  if (freetype.empty()) {
    logger::error("cap", "create freetype2 instance failed");
    stop(false);
    return;
  }
  freetype->loadFontData("Helvetica.ttc", 0);

  //准备 capture 和 decoder(如果需要)
#ifdef USE_V4L_CAPTURE
  //改了这里也要去改do decode那里
  // soft_jpg decoder;
  hard_jpg decoder;
  v4l_capture capture;
  if (capture.open(config.device,
                   v4l_capture::graphic{(uint32_t)config.resolution.width,
                                        (uint32_t)config.resolution.height,
                                        (uint32_t)config.fps,
                                        config.cam_pix_fmt})) {
    logger::error("cap", "v4l2 VideoCapture open failed");
    stop();
    return;
  }
#else
  cv::VideoCapture capture;
  if (config.device.empty()) {
    if (!capture.open(0)) {
      logger::fatal("cap", "cv videocapture open failed");
      stop(false);
      return;
    }
  } else {
    if (!capture.open(config.device)) {
      logger::fatal("cap", "cv videocapture open failed");
      stop(false);
      return;
    }
  }
  char decoder;
#endif

//准备writer
OPEN_WRITER:
  cv::VideoWriter writer;
  time_t task_begin_time;
  time(&task_begin_time);
  uint64_t task_begin_mono_time = checkpoint(3);
  const std::string video_filename =
      make_filename(config.save_location, task_begin_time) + "." +
      codec_file_format(config.output_codec);
  const std::string preview_filename =
      make_filename(config.save_location, task_begin_time) + ".jpg";

  auto do_log = [this, task_begin_time, video_filename, preview_filename,
                 config] {
    auto localt = localtime(&task_begin_time);
    log.add(video_filename, preview_filename, config.duration, mktime(localt),
            true);
    if (!write_log(_config.save_location, log.to_str())) {
      logger::error("cap", "write log failed");
    }
  };

  bool preview_saved = false;

  //试三次打开writer
  static const int open_writer_retry = 3;
  for (int tried = 0; tried < open_writer_retry; tried++) {
    if (!writer.open(video_filename, codec_fourcc(config.output_codec),
                     config.fps, config.resolution)) {
      logger::error("cap", "VideoWriter open ", video_filename, " failed ",
                    tried + 1);
    } else {
      break;
    }
  }
  if (!writer.isOpened()) {
    logger::fatal("cap", "can not open VideoWriter");
    goto QUIT;
  }

  logger::info("cap", "duration: ", config.duration,
               " seconds, change video file to ", video_filename);

  while (true) {
    if (state == STOPPING) {
      goto QUIT;
    }

    frame_context ctx;
    if (!do_capture(config, ctx, &capture)) {
      goto QUIT;
    }

    //保存jpg预览图
    if (!preview_saved) {
      FILE *fp = fopen(preview_filename.c_str(), "wb");
      if (!fp) {
        logger::fatal("cap", "do_capture write jpg preview failed(fopen)");
        goto QUIT;
      }
      auto actual_write =
          fwrite(ctx.captured_frame->data, 1, ctx.captured_frame->length, fp);
      if (actual_write != ctx.captured_frame->length) {
        logger::fatal("cap", "do_capture write jpg preview failed(fwrite)");
        fclose(fp);
        goto QUIT;
      }
      fclose(fp);
      preview_saved = true;
    }

    //解码jpg
    if (!do_decode(config, ctx, &decoder)) {
      goto QUIT;
    }
    //编码avc
    if (!do_write(config, ctx, video_filename, freetype.get(), writer)) {
      goto QUIT;
    }
    frame_cnt++;
    //到了预定的时间，换文件
    if (ctx.send_time - task_begin_mono_time >= config.duration * 1000) {
      do_log();
      goto OPEN_WRITER;
    }
  }
QUIT:
  do_log();
  stop(false);
}

// FIXME foreach session的代码应该复用
bool capture::do_capture(const config &config, frame_context &ctx,
                         void *raw_capture) {
#ifdef USE_V4L_CAPTURE
  auto capture = (v4l_capture *)raw_capture;
  auto process = [capture, this](frame_context &ctx) -> bool {
    ctx.capture_time = checkpoint(3);
    time(&ctx.frame_time);
    std::pair<bool, std::shared_ptr<v4l_capture::buffer>> packet =
        capture->read();
    if (!packet.first) {
      logger::error("cap", "VideoCapture read failed");
      return false;
    }
    ctx.captured_frame = std::move(packet.second);
    ctx.send_time = checkpoint(3);

    std::vector<unsigned char> out(ctx.captured_frame->length,
                                   (unsigned char)0);
    memcpy(out.data(), ctx.captured_frame->data, ctx.captured_frame->length);
    web_service.foreach_session([&out](const web::session_context &_session) {
      auto shared_session = _session.session.lock();
      if (shared_session) {
        auto copy = out;
        if (_session.ssl) {
          static_cast<session<true> *>(shared_session.get())
              ->ws_write(std::move(copy), true, 1);
        } else {
          static_cast<session<false> *>(shared_session.get())
              ->ws_write(std::move(copy), true, 1);
        }
      }
    });

    ctx.send_done_time = checkpoint(3);

    return true;
  };
#else
  auto capture = (cv::VideoCapture *)raw_capture;
  auto process = [capture, this](frame_context &ctx) -> bool {
    ctx.capture_time = checkpoint(3);
    time(&ctx.frame_time);
    cv::Mat mat;
    if (!capture->read(mat)) {
      logger::error("cap", "!VideoCapture read failed");
      return false;
    }
    // FIXME 这里有奇怪的问题，鬼知道咋回事
    cv::normalize(mat, ctx.decoded_frame, 0, 255, cv::NORM_MINMAX, CV_8U);
    /*ctx.decoded_frame = std::move(mat);*/

    /* FIXME
     * 这个地方应该想办法拿到opencv拿到的原始jpg，而不是在这重新encode一个jpg
     */
    std::vector<unsigned char> out;
    cv::imencode(".jpg", ctx.decoded_frame, out);

    ctx.captured_frame = std::shared_ptr<v4l_capture::buffer>(
        (v4l_capture::buffer *)malloc(sizeof(v4l_capture::buffer) + out.size()),
        [](v4l_capture::buffer *p) { free(p); });
    ctx.captured_frame->data =
        (char *)(ctx.captured_frame.get()) + sizeof(v4l_capture::buffer);
    ctx.captured_frame->length = out.size();
    memcpy(ctx.captured_frame->data, out.data(), ctx.captured_frame->length);

    ctx.send_time = checkpoint(3);
    web_service.foreach_session([&out](const web::session_context &_session) {
      auto shared_session = _session.session.lock();
      if (shared_session) {
        auto copy = out;
        if (_session.ssl) {
          static_cast<session<true> *>(shared_session.get())
              ->ws_write(std::move(copy), true, 1);
        } else {
          static_cast<session<false> *>(shared_session.get())
              ->ws_write(std::move(copy), true, 1);
        }
      }
    });
    ctx.send_done_time = checkpoint(3);
    return true;
  };
#endif

  if (!process(ctx)) {
    return false;
  }
  return true;
}

bool capture::do_decode(const config &config, frame_context &ctx,
                        void *decoder) {
#ifdef USE_V4L_CAPTURE
  hard_jpg *jpg_decoder = (hard_jpg *)decoder;

  auto process = [jpg_decoder](frame_context &ctx) -> bool {
    ctx.decode_time = checkpoint(3);
    std::pair<bool, cv::Mat> decoded =
        jpg_decoder->decode((unsigned char *)(ctx.captured_frame->data),
                            ctx.captured_frame->length);
    if (!decoded.first) {
      logger::error("cap", "JPG decode failed");
      return false;
    }
    ctx.decoded_frame = std::move(decoded.second);
    ctx.captured_frame.reset();
    ctx.decode_done_time = checkpoint(3);
    return true;
  };
#else
  auto process = [](frame_context &ctx) -> bool {
    ctx.decode_time = checkpoint(3);
    ctx.decode_done_time = ctx.decode_time;
    return true;
  };
#endif

  if (!process(ctx)) {
    return false;
  }
  return true;
}

bool capture::do_write(const config &config, frame_context &ctx,
                       const std::string &video_filename,
                       cv::freetype::FreeType2 *freetype,
                       cv::VideoWriter &writer) {
  const uint32_t expect_frame_time = 1000 / config.fps;

  cv::Mat &frame = ctx.decoded_frame;
  ctx.process_time = checkpoint(3);
  {
    //在指定位置渲染时间
    auto localt = localtime(&ctx.frame_time);
    std::ostringstream fmt(std::ios::app);
    fmt << localt->tm_year + 1900 << '/' << localt->tm_mon + 1 << '/'
        << localt->tm_mday << " " << localt->tm_hour << ':' << localt->tm_min
        << ':' << localt->tm_sec;

    render_text(config.timestamp_pos, fmt.str(), config.font_height,
                std::optional<cv::Scalar>(), freetype, frame);
    //渲染帧率
    fmt.str("");
    //测速周期（秒）
    static const uint32_t period = 1;
    if (frame_cost != 0) {
      uint32_t current_time = checkpoint(3);
      if (base_time == 0 || current_time - base_time > 1000 * period) {
        display_fps = frame_cnt - frame_cnt_base;
        low_fps = display_fps < config.fps * 95 / 100;
        if (low_fps) {
          logger::warn("cap", "frame drop detected, fps: ", display_fps);
        }

        base_time = current_time;
        frame_cnt_base = frame_cnt;
      }

      if (low_fps) {
        if (config.display_fps == 1 || config.display_fps == 2) {
          fmt << "FPS: ";
          fmt << display_fps;
        }
      } else {
        if (config.display_fps == 2) {
          fmt << "FPS: ";
          fmt << display_fps;
        }
      }
      if (fmt.str() != "") {
        render_text((config.timestamp_pos + 1) % 4, fmt.str(),
                    config.font_height,
                    low_fps ? cv::Scalar((double)30, (double)120, (double)238)
                            : std::optional<cv::Scalar>(),
                    freetype, frame);
      }
    }
  }

  ctx.write_time = checkpoint(3);
  try {
    writer.write(frame);
  } catch (const cv::Exception &ex) {
    logger::fatal("cap", "writer.write failed");
    return false;
  } catch (const std::exception &ex) {
    logger::fatal("cap", "writer.write failed");
    return false;
  } catch (...) {
    logger::fatal("cap", "writer.write failed");
    return false;
  }
  ctx.done_time = checkpoint(3);

  /*frame_cost = ctx.send_time - ctx.capture_time;
  if (ctx.decode_done_time - ctx.decode_time > frame_cost) {
    frame_cost = ctx.decode_done_time - ctx.decode_time;
  }
  if (ctx.done_time - ctx.process_time > frame_cost) {
    frame_cost = ctx.done_time - ctx.process_time;
  }*/
  frame_cost = ctx.done_time - ctx.capture_time;
  if (frame_cost > expect_frame_time) {
    logger::debug("cap", "low frame rate, expect ", expect_frame_time,
                  "ms, actual ",
                  frame_cost, //
                  "ms (capture:", ctx.send_time - ctx.capture_time,
                  "ms, send:", ctx.send_done_time - ctx.send_time,
                  "ms, inter-thread:", ctx.decode_time - ctx.send_done_time,
                  "ms, decode:", ctx.decode_done_time - ctx.decode_time,
                  "ms, inter-thread:", ctx.process_time - ctx.decode_done_time,
                  "ms, process:", ctx.write_time - ctx.process_time,
                  "ms, write:", ctx.done_time - ctx.write_time, "ms)");
  } else {
    logger::debug("cap", "cost ", frame_cost, " ms");
  }
  return true;
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