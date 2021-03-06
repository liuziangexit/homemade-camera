#include "video/capture.h"
#include "config/config.h"
#include "util/logger.h"
#include "util/time_util.h"
#include "video/codec.h"
#include "video/soft_jpg.h"
#include "web/session.h"
#include <algorithm>
#include <atomic>
#include <future>
#include <ipc/ipc.h>
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

std::string map_fs(const std::string &base, const std::string &file) {
  std::string result(base);
  if (base.back() != '/') {
    result.append("/");
  }
  if (file.front() == '/') {
    result.append(file.substr(1, file.size()));
  } else {
    result.append(file);
  }
  return result;
}

capture::capture(int cap_web_fd)
    : _config(config::get()), cap_web_fd(cap_web_fd) {}
capture::~capture() { stop(); }

void capture::run(int ipc_fd) {
  {
    int expect = STOPPED;
    if (!state.compare_exchange_strong(expect, RUNNING))
      throw std::logic_error("capture already running");
  }
  auto json_string = read_log(_config.save_location);
  if (!log.parse(json_string)) {
    logger::fatal("log parse failed");
    abort();
  }

  directory_size = 0;
  for (const auto &p : log.rows) {
    bool succ;
    directory_size +=
        file_length(map_fs(config::get().save_location, p.filename), succ);
    if (!succ) {
      logger::error("can not retrieve info of ",
                    map_fs(config::get().save_location, p.filename));
      internal_stop_avoid_deadlock();
      return;
    }
    directory_size +=
        file_length(map_fs(config::get().save_location, p.preview), succ);
    if (!succ) {
      logger::error("can not retrieve info of ",
                    map_fs(config::get().save_location, p.preview));
      internal_stop_avoid_deadlock();
      return;
    }
  }

  capture_thread = std::thread([this] { this->do_capture(this->_config); });
  decode_thread = std::thread([this] { this->do_decode(this->_config); });
  write_thread = std::thread([this] { this->do_write(this->_config); });

  //处理ipc
  while (state == RUNNING) {
    if (0 == ipc::wait(ipc_fd, 1000)) {
      //这里是为了每秒都检查state这个标志，如果其他线程都退出了，那么这个循环也该退出了
      continue;
    }

    auto msg = ipc::recv(ipc_fd);
    if (msg.first) {
      logger::error("ipc handler read failed, quitting...", msg.first);
      return;
    }
    std::string text((char *)msg.second.content, msg.second.size);
    if (text == "PING") {
      //心跳
      if (ipc::send(ipc_fd, "PONG")) {
        // something goes wrong
        return;
      }
    } else if (text == "EXIT") {
      logger::debug("IPC EXIT, quitting...");
      return;
    }
  }
}

void capture::stop() {
  logger::info("stopping capture");
  {
    int expect = RUNNING;
    if (!state.compare_exchange_strong(expect, STOPPING)) {
      logger::debug("stop failed, expect state ", RUNNING, ", actual state ",
                    expect);
      return;
    }
  }
  if (!write_log(_config.save_location, log.to_str())) {
    logger::error("write log failed");
  }
  if (std::this_thread::get_id() != capture_thread.get_id()) {
    if (capture_thread.joinable())
      capture_thread.join();
  }
  if (std::this_thread::get_id() != decode_thread.get_id()) {
    if (decode_thread.joinable())
      decode_thread.join();
  }
  if (std::this_thread::get_id() != write_thread.get_id()) {
    if (write_thread.joinable())
      write_thread.join();
  }
  state = STOPPED;
  logger::info("capture stopped");
}

void capture::internal_stop_avoid_deadlock() {
  std::thread([this] { stop(); }).detach();
}

#define WARN_QUEUE_CNT 3
#define PAUSE_QUEUE_CNT 60

bool capture::pause_others() {
  std::unique_lock l(pause_mtx);
  if (paused)
    return false;
  paused = true;
  pause_time = checkpoint(3);
  logger::warn("pause others!");
  return true;
}

void capture::resume_others() {
  std::unique_lock l(pause_mtx);
  paused = false;
  pause_cv.notify_all();
  logger::warn(
      "cap", "resume others! paused time: ", checkpoint(3) - pause_time, "ms");
}

void capture::wait_pause() {
  std::unique_lock l(pause_mtx);
  bool pause_requested = false;
  if (paused) {
    pause_requested = true;
    logger::warn("wait pause!");
  }
  pause_cv.wait(l, [this] { return !paused; });
  if (pause_requested)
    logger::warn("resumed!");
}

void capture::do_capture(const config &config) {
  int sock_snd_buffer_len;
  socklen_t optlen = sizeof(sock_snd_buffer_len);

  if (getsockopt(cap_web_fd, SOL_SOCKET, SO_SNDBUF, &sock_snd_buffer_len,
                 &optlen) == -1) {
    this->internal_stop_avoid_deadlock();
    return;
  }

  auto auto_adjust_socket_snd_buffer = [&sock_snd_buffer_len,
                                        this](int jpg_len) {
    if (sock_snd_buffer_len < jpg_len * 2) {
      //这里需要把sock的send缓冲区开到比图片还要大，不然的话有可能send时候就阻塞直到那边开始read了
      //如果那边read还好说，如果那边都崩溃了，没人去read，这个send就卡住了！
      //为什么不应async的sock呢，因为这种unix sock没有async实现！
      // FIXME 这里会不会整型溢出？先不管
      sock_snd_buffer_len = jpg_len * 2;
      if (-1 == setsockopt(cap_web_fd, SOL_SOCKET, SO_SNDBUF,
                           &sock_snd_buffer_len, sizeof(sock_snd_buffer_len))) {
        return false;
      }
    }
    return true;
  };

#ifdef USE_V4L_CAPTURE
  v4l_capture capture;
  if (capture.open(config.device,
                   v4l_capture::graphic{(uint32_t)config.resolution.width,
                                        (uint32_t)config.resolution.height,
                                        (uint32_t)config.fps,
                                        config.cam_pix_fmt})) {
    logger::error("VideoCapture open failed");
    this->internal_stop_avoid_deadlock();
    return;
  }

  auto process = [&capture, this,
                  auto_adjust_socket_snd_buffer](frame_context &ctx) -> bool {
    ctx.capture_time = checkpoint(3);
    time(&ctx.frame_time);
    std::pair<bool, std::shared_ptr<v4l_capture::buffer>> packet =
        capture.read();
    if (!packet.first) {
      logger::error("VideoCapture read failed");
      return false;
    }
    ctx.captured_frame = std::move(packet.second);
    ctx.send_time = checkpoint(3);

    std::vector<unsigned char> out(ctx.captured_frame->length,
                                   (unsigned char)0);
    memcpy(out.data(), ctx.captured_frame->data, ctx.captured_frame->length);

    if (!auto_adjust_socket_snd_buffer(ctx.captured_frame->length)) {
      return false;
    }
    auto ret = ipc::wait(cap_web_fd, 0);
    if (ret == 1) {
      auto ready_msg = ipc::recv(cap_web_fd);
      if (ready_msg.first == 0) {
        std::string text((char *)ready_msg.second.content,
                         ready_msg.second.size);
        if (text == "READY") {
          if (ipc::send(cap_web_fd, out.data(), ctx.captured_frame->length)) {
            logger::warn("send frame to web failed");
          }
        }
      }
    }

    ctx.send_done_time = checkpoint(3);

    return true;
  };
#else
  logger::info("opening cv::VideoCapture");
  cv::VideoCapture capture;
  if (config.device.empty()) {
    if (!capture.open(0))
      throw std::runtime_error("capture open failed");
  } else {
    if (!capture.open(config.device))
      throw std::runtime_error("capture open failed");
  }
  logger::info("cv::VideoCapture opened");

  auto process = [&capture, this,
                  auto_adjust_socket_snd_buffer](frame_context &ctx) -> bool {
    ctx.capture_time = checkpoint(3);
    time(&ctx.frame_time);
    cv::Mat mat;
    if (!capture.read(mat)) {
      logger::error("!VideoCapture read failed");
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
    if (!auto_adjust_socket_snd_buffer(ctx.captured_frame->length)) {
      return false;
    }
    auto ret = ipc::wait(cap_web_fd, 0);
    if (ret == 1) {
      auto ready_msg = ipc::recv(cap_web_fd);
      if (ready_msg.first == 0) {
        std::string text((char *)ready_msg.second.content,
                         ready_msg.second.size);
        if (text == "READY") {
          if (ipc::send(cap_web_fd, out.data(), ctx.captured_frame->length)) {
            logger::warn("send frame to web failed");
          }
        }
      }
    }
    ctx.send_done_time = checkpoint(3);
    return true;
  };
#endif

  while (true) {
    if (state == STOPPING) {
      break;
    }

    wait_pause();

    frame_context ctx;
    if (!process(ctx)) {
      break;
    }
    {
      if (save_preview) {
        FILE *fp = fopen(preview_filename.c_str(), "wb");
        if (!fp) {
          logger::error("do_capture write jpg preview failed(fopen)");
          this->internal_stop_avoid_deadlock();
          return;
        }
        auto actual_write =
            fwrite(ctx.captured_frame->data, 1, ctx.captured_frame->length, fp);
        if (actual_write != ctx.captured_frame->length) {
          logger::error("do_capture write jpg preview failed(fwrite)");
          fclose(fp);
          this->internal_stop_avoid_deadlock();
          return;
        }
        fclose(fp);

        bool expect = true;
        auto swap_ok = save_preview.compare_exchange_strong(expect, false);
        assert(swap_ok);
      }
    }
    std::unique_lock l(capture2decode_mtx);
    if (capture2decode_queue.size() > PAUSE_QUEUE_CNT) {
      continue;
    }
    capture2decode_queue.push(std::move(ctx));
    capture2decode_cv.notify_one();
  }
  std::unique_lock l(capture2decode_mtx);
  frame_context ctx;
  ctx.quit = true;
  capture2decode_queue.push(ctx);
  capture2decode_cv.notify_one();
  this->internal_stop_avoid_deadlock();
}

void capture::do_decode(const config &config) {
#ifdef USE_V4L_CAPTURE
  soft_jpg jpg_decoder;

  auto process = [&jpg_decoder](frame_context &ctx) -> bool {
    ctx.decode_time = checkpoint(3);
    std::pair<bool, cv::Mat> decoded =
        jpg_decoder.decode((unsigned char *)(ctx.captured_frame->data),
                           ctx.captured_frame->length);
    if (!decoded.first) {
      logger::error("JPG decode failed");
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

  bool paused_by_me = false;
  while (true) {
    if (!paused_by_me)
      wait_pause();

    std::unique_lock lc2d(capture2decode_mtx);
    capture2decode_cv.wait(lc2d,
                           [this] { return !capture2decode_queue.empty(); });
    frame_context ctx = std::move(capture2decode_queue.front());
    capture2decode_queue.pop();
    if (ctx.quit) {
      break;
    }
    auto capture2decode_queue_size = capture2decode_queue.size();
    if (capture2decode_queue_size > WARN_QUEUE_CNT) {
      logger::warn("capture2decode_queue size: ", capture2decode_queue_size);
    }
    lc2d.unlock();

    if (capture2decode_queue_size >= PAUSE_QUEUE_CNT) {
      if (!paused_by_me) {
        if (pause_others())
          paused_by_me = true;
      }
    } else if (capture2decode_queue_size == 0) {
      if (paused_by_me) {
        resume_others();
        paused_by_me = false;
      }
    }

    if (!process(ctx)) {
      //运行久了之后解码可能会收到损坏的jpg，暂时不清楚这个是因为什么
      //我猜可能是bit flip之类的
      continue;
      /*break;*/
    }

    std::unique_lock ld2w(decode2write_mtx);
    if (decode2write_queue.size() > PAUSE_QUEUE_CNT) {
      continue;
    }
    decode2write_queue.push(std::move(ctx));
    decode2write_cv.notify_one();
  }
  std::unique_lock l(decode2write_mtx);
  frame_context ctx;
  ctx.quit = true;
  decode2write_queue.push(ctx);
  decode2write_cv.notify_one();
  this->internal_stop_avoid_deadlock();
}

void capture::do_write(const config &config) {
  static const int open_writer_retry = 3;

  if (config.duration < 1)
    this->stop(); //短于1秒的话文件名可能重复

  auto fps = (uint32_t)config.fps;
  cv::Size frame_size(config.resolution);
  const uint32_t expect_frame_time = 1000 / fps;

  cv::Ptr<cv::freetype::FreeType2> freetype = cv::freetype::createFreeType2();
  if (freetype.empty()) {
    logger::error("create freetype2 instance failed");
    this->stop();
  }
  freetype->loadFontData("Helvetica.ttc", 0);

  cv::VideoWriter writer;
  auto prepare_write = [this, config, frame_size,
                        fps](time_t date, cv::VideoWriter &writer,
                             std::string &out_filename) {
    auto filename = make_filename(config.save_location, date);
    std::string video_filename =
        filename + "." + codec_file_format(config.output_codec);

    //通知do_capture去保存一个jpg
    auto prev = save_preview.exchange(true);
    assert(prev == false);
    preview_filename = filename + ".jpg";

    //试三次打开writer
    for (int tried = 0; tried < open_writer_retry; tried++) {
      if (writer.open(video_filename, codec_fourcc(config.output_codec), fps,
                      frame_size)) {
        out_filename = video_filename;
        return true;
      } else {
        logger::error("VideoWriter open ", video_filename, " failed ",
                      tried + 1);
      }
    }
    return false;
  };

OPEN_WRITER:
  if (directory_size >= (uint64_t)config::get().max_storage * (uint64_t)1024 *
                            (uint64_t)1024 * (uint64_t)1024) {
    logger::info("max_storage limit reached(", directory_size,
                 "), pause and removing old video(s)");
    pause_others();
    while (directory_size >= (uint64_t)config::get().max_storage *
                                 (uint64_t)1024 * (uint64_t)1024 *
                                 (uint64_t)1024) {
      if (log.rows.size() == 1) {
        //防止删到正在写的文件
        logger::error("running out of disk space");
        resume_others();
        internal_stop_avoid_deadlock();
        return;
      }
      auto video_info = log.pop_front();
      bool succ;
      auto total_removed = file_length(
          map_fs(config::get().save_location, video_info.preview), succ);
      if (!succ) {
        logger::error("can not retrieve info of ",
                      map_fs(config::get().save_location, video_info.preview));
        resume_others();
        internal_stop_avoid_deadlock();
        return;
      }
      total_removed += file_length(
          map_fs(config::get().save_location, video_info.filename), succ);
      if (!succ) {
        logger::error("can not retrieve info of ",
                      map_fs(config::get().save_location, video_info.filename));
        resume_others();
        internal_stop_avoid_deadlock();
        return;
      }

      if (remove(map_fs(config::get().save_location, video_info.filename)
                     .c_str()) == 0) {
        logger::info("remove ",
                     map_fs(config::get().save_location, video_info.filename),
                     " ok");
      } else {
        logger::error("remove ",
                      map_fs(config::get().save_location, video_info.filename),
                      " failed");
        resume_others();
        internal_stop_avoid_deadlock();
        return;
      }
      if (remove(map_fs(config::get().save_location, video_info.preview)
                     .c_str()) == 0) {
        logger::info("remove ",
                     map_fs(config::get().save_location, video_info.preview),
                     " ok");
      } else {
        logger::error("remove ",
                      map_fs(config::get().save_location, video_info.preview),
                      " failed");
        resume_others();
        internal_stop_avoid_deadlock();
        return;
      }
      directory_size -= total_removed;
    }
    logger::info("necessarily disk space reclaimed");
    resume_others();
  }

  time_t tm;
  time(&tm);
  std::string filename;
  if (!prepare_write(tm, writer, filename)) {
    logger::error("prepare_writer failed");
    this->stop();
  }

  auto do_log = [this, tm, filename, config] {
    auto localt = localtime(&tm);
    log.add(filename.substr(filename.find('/') + 1, filename.size()),
            preview_filename.substr(preview_filename.find('/') + 1,
                                    preview_filename.size()),
            config.duration, mktime(localt), true);
    if (!write_log(_config.save_location, log.to_str())) {
      logger::error("write log failed");
    }
  };

  uint64 task_begin = 0;
  logger::info("video file change to ", filename);
  logger::debug("writer backend:", writer.getBackendName(),
                " codec:", codec_to_string(config.output_codec), " fps:", fps,
                " resolution:", frame_size);
  do_log();

  bool paused_by_me = false;
  while (true) {
    if (!paused_by_me)
      wait_pause();

    std::unique_lock l(decode2write_mtx);
    decode2write_cv.wait(l, [this] { return !decode2write_queue.empty(); });
    frame_context ctx = std::move(decode2write_queue.front());
    decode2write_queue.pop();
    if (ctx.quit) {
      break;
    }
    auto decode2write_queue_size = decode2write_queue.size();
    if (decode2write_queue_size > WARN_QUEUE_CNT) {
      logger::warn("decode2write_queue size: ", decode2write_queue_size);
    }
    l.unlock();

    if (decode2write_queue_size >= PAUSE_QUEUE_CNT) {
      if (!paused_by_me) {
        if (pause_others())
          paused_by_me = true;
      }
    } else if (decode2write_queue_size == 0) {
      if (paused_by_me) {
        resume_others();
        paused_by_me = false;
      }
    }

    if (task_begin == 0) {
      task_begin = ctx.send_time;
    }
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
          low_fps = display_fps < fps * 95 / 100;
          if (low_fps) {
            logger::warn("frame drop detected, fps: ", display_fps);
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
      logger::error("writer.write failed");
      internal_stop_avoid_deadlock();
      break;
    } catch (const std::exception &ex) {
      logger::error("writer.write failed");
      internal_stop_avoid_deadlock();
      break;
    } catch (...) {
      logger::error("writer.write failed");
      internal_stop_avoid_deadlock();
      break;
    }
    ctx.done_time = checkpoint(3);

    frame_cost = ctx.send_time - ctx.capture_time;
    if (ctx.decode_done_time - ctx.decode_time > frame_cost) {
      frame_cost = ctx.decode_done_time - ctx.decode_time;
    }
    if (ctx.done_time - ctx.process_time > frame_cost) {
      frame_cost = ctx.done_time - ctx.process_time;
    }

    if (frame_cost > expect_frame_time) {
      /* logger::debug(
           "cap", "low frame rate, expect ", expect_frame_time, "ms, actual ",
           frame_cost, //
           "ms (capture:", ctx.send_time - ctx.capture_time,
           "ms, send:", ctx.send_done_time - ctx.send_time,
           "ms, inter-thread:", ctx.decode_time - ctx.send_done_time,
           "ms, decode:", ctx.decode_done_time - ctx.decode_time,
           "ms, inter-thread:", ctx.process_time - ctx.decode_done_time,
           "ms, process:", ctx.write_time - ctx.process_time,
           "ms, write:", ctx.done_time - ctx.write_time, "ms)");*/
    } else {
      /*logger::debug( "cost ", frame_cost, "ms");*/
    }

    frame_cnt++;

    //到了预定的时间，换文件
    if (ctx.send_time - task_begin >= config.duration * 1000) {
      bool succ;
      directory_size += file_length(filename, succ);
      if (!succ) {
        logger::error("can not retrieve info of ", filename);
        internal_stop_avoid_deadlock();
        return;
      }
      directory_size += file_length(preview_filename, succ);
      if (!succ) {
        logger::error("can not retrieve info of ", preview_filename);
        internal_stop_avoid_deadlock();
        return;
      }
      goto OPEN_WRITER;
    }
  }
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