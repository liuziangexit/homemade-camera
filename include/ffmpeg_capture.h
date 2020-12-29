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
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace homemadecam {

struct device_info {
  std::vector<cv::Size> resolution;
};

class ffmpeg_capture {
  AVFormatContext *format_context;
  AVInputFormat *input_format;
  AVCodecContext *codec_context;
  AVCodec *codec;
  AVFrame *grab_frame = nullptr;
  AVPacket *packet;

public:
  ffmpeg_capture() { avdevice_register_all(); }

  int open_device(const std::string &api, const std::string &device,
                  cv::Size resolution, int framerate) {
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

  cv::Mat grab() {
    if (!grab_frame) {
      grab_frame = av_frame_alloc();
      if (!grab_frame) {
        throw std::bad_alloc();
      }
    } else {
      av_frame_unref(grab_frame);
    }
    if (!packet) {
      packet = av_packet_alloc();
      if (!packet) {
        throw std::bad_alloc();
      }
    } else {
      av_init_packet(packet);
    }

    if (av_read_frame(format_context, packet) < 0)
      throw std::runtime_error("av_read_frame");

    int got_frame = 0;
    if (avcodec_decode_video2(codec_context, grab_frame, &got_frame, packet) <
        0) {
      got_frame = 0;
    }
    if (!got_frame) {
      throw std::runtime_error("avcodec_decode_video2");
    }

    return avframe_to_cvmat(grab_frame);
  }

  void close_device() {
    avcodec_close(codec_context);
    AVCodec *codec;
    avformat_close_input(&format_context);
    avformat_free_context(format_context);
    av_frame_free(&grab_frame);
    av_packet_free(&packet);
  }

private:
  static std::string size_to_string(cv::Size s) {
    std::ostringstream fmt;
    fmt << s.width << "x" << s.height;
    return std::string(fmt.str());
  }

  // AVFrame 转 cv::mat
  static cv::Mat avframe_to_cvmat(const AVFrame *frame) {
    int width = frame->width;
    int height = frame->height;
    cv::Mat image(height, width, CV_8UC3);
    int cvLinesizes[1];
    cvLinesizes[0] = image.step1();
    SwsContext *conversion = sws_getContext(
        width, height, (AVPixelFormat)frame->format, width, height,
        AVPixelFormat::AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    sws_scale(conversion, frame->data, frame->linesize, 0, height, &image.data,
              cvLinesizes);
    sws_freeContext(conversion);
    return image;
  }
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
