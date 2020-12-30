#ifndef __HOMECAM_FFMPEG_ENCODER_H_
#define __HOMECAM_FFMPEG_ENCODER_H_
#include <ostream>
#include <type_traits>
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
#include "codec.h"
#include <type_traits>

namespace homemadecam {

template <typename STREAM> class ffmpeg_encoder {

  typename std::decay<STREAM>::type *stream_;

  AVFormatContext *format_context;
  AVCodecContext *codec_context;
  AVCodec *encoder;
  AVPacket *packet;
  AVIOContext *io_context;
  AVStream *av_stream;
  unsigned char *io_context_buf;

public:
  ffmpeg_encoder() {
    av_register_all();
    avcodec_register_all();
  }
  ~ffmpeg_encoder() { close(); }

  int open(codec fmt, AVCodecContext *decoder_context, STREAM &stream,
           const std::size_t io_buf_len = 1024 * 1024 * 4) {
    close();
    stream_ = &stream;

    encoder = avcodec_find_encoder(codec_id(fmt));
    if (!encoder) {
      return -99;
    }

    if (avformat_alloc_output_context2(&format_context, nullptr,
                                       codec_to_string(fmt).c_str(),
                                       nullptr) < 0) {
      return -1;
    }

    io_context_buf = (unsigned char *)av_malloc(io_buf_len);
    io_context = avio_alloc_context(io_context_buf, io_buf_len, 1, &stream_,
                                    NULL, nullptr, NULL);
    if (!io_context) {
      return -2;
    }
    format_context->pb = io_context;
    format_context->flags = AVFMT_FLAG_CUSTOM_IO;

    av_stream = avformat_new_stream(format_context, nullptr);
    if (!av_stream) {
      return -3;
    }

    codec_context = av_stream->codec;
    codec_context->height = decoder_context->height;
    codec_context->width = decoder_context->width;
    codec_context->sample_aspect_ratio = decoder_context->sample_aspect_ratio;
    codec_context->pix_fmt = encoder->pix_fmts[0];
    codec_context->time_base = decoder_context->time_base;
    codec_context->me_range = decoder_context->me_range;
    codec_context->max_qdiff = decoder_context->max_qdiff;
    codec_context->qmin = decoder_context->qmin;
    codec_context->qmax = decoder_context->qmax;
    codec_context->qcompress = decoder_context->qcompress;
    codec_context->refs = decoder_context->refs;
    codec_context->bit_rate = decoder_context->bit_rate;
    /*
    codec_context->me_range = 16;
    codec_context->max_qdiff = 4;
    codec_context->qmin = 10;
    codec_context->qmax = 51;
    codec_context->qcompress = 0.6;
    codec_context->refs = 3;
    codec_context->bit_rate = 500000;
     */

    if (avcodec_open2(codec_context, encoder, NULL) < 0) {
      return -4;
    }

    return 0;
  }
  int write() {}
  void close() {}

private:
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
