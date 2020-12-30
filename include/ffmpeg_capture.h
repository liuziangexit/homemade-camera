#ifndef __HOMECAM_FFMPEG_CAPTURE_H_
#define __HOMECAM_FFMPEG_CAPTURE_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif
#include <guard.h>
#include <opencv2/core/types.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace homemadecam {

class ffmpeg_capture {
  AVFormatContext *format_context = nullptr;
  AVInputFormat *input_format = nullptr;
  AVCodec *codec = nullptr;
  AVFrame *grab_frame = nullptr;

public:
  AVCodecContext *codec_context = nullptr;

  ffmpeg_capture() {
    avdevice_register_all();
    av_register_all();
    avcodec_register_all();
  }

  ~ffmpeg_capture() { close(); }

  int open(const std::string &api, const std::string &device,
           cv::Size resolution, int framerate) {
    close();

    format_context = avformat_alloc_context();
    if (!format_context) {
      return -1;
    }
    input_format = av_find_input_format(api.c_str());
    if (!input_format) {
      return -2;
    }
    AVDictionary *params = nullptr;
    av_dict_set(&params, "video_size", size_to_string(resolution).c_str(), 0);
    av_dict_set(&params, "framerate", std::to_string(framerate).c_str(), 0);
    av_dict_set(&params, "pixel_format", "uyvy422", 0);

    if (avformat_open_input(&format_context, device.c_str(), input_format,
                            &params)) {
      return -3;
    }

    av_dict_free(&params);

    if (avformat_find_stream_info(format_context, nullptr) < 0)
      return -1;

    // find the stream index, we'll only be encoding the video for now
    int video_stream_idx = -1;
    for (int i = 0; i < format_context->nb_streams; i++) {
      if (format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
        video_stream_idx = i;
        break;
      }
    }
    if (video_stream_idx == -1)
      return -4;

    codec_context = format_context->streams[video_stream_idx]->codec;
    codec = avcodec_find_decoder(codec_context->codec_id);
    if (!codec)
      return -5;

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
      return -6;
    }
    return 0;
  }

  //这个返回的指针指向的对象在下一次grab前或capture对象析构前有效
  AVFrame *read() {
    if (!grab_frame) {
      grab_frame = av_frame_alloc();
      if (!grab_frame) {
        throw std::bad_alloc();
      }
    } else {
      av_frame_unref(grab_frame);
    }
    AVPacket packet;
    av_init_packet(&packet);

    if (av_read_frame(format_context, &packet) < 0)
      throw std::runtime_error("av_read_frame");

    int got_frame = 0;
    if (avcodec_decode_video2(codec_context, grab_frame, &got_frame, &packet) <
        0) {
      got_frame = 0;
    }
    if (!got_frame) {
      throw std::runtime_error("avcodec_decode_video2");
    }

    return grab_frame;
  }

  void close() {
    if (codec_context)
      avcodec_close(codec_context);
    codec_context = nullptr;
    if (format_context) {
      avformat_free_context(format_context);
      format_context = nullptr;
    }
    input_format = nullptr;
    codec = nullptr;
    if (grab_frame) {
      av_frame_free(&grab_frame);
      grab_frame = nullptr;
    }
  }

private:
  static std::string size_to_string(cv::Size s) {
    std::ostringstream fmt;
    fmt << s.width << "x" << s.height;
    return std::string(fmt.str());
  }
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
