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
#include <istream>
#include <type_traits>

namespace homemadecam {

template <bool UNSIGNED = false> class ffmpeg_encoder {

  using stream_type = std::conditional_t<UNSIGNED,                          //
                                         std::basic_ostream<unsigned char>, //
                                         std::basic_ostream<char>>;
  stream_type *stream_;

  AVFormatContext *format_context = nullptr;
  AVCodecContext *codec_context = nullptr;
  AVCodec *encoder = nullptr;
  AVIOContext *io_context = nullptr;
  AVStream *av_stream = nullptr;
  unsigned char *io_context_buf = nullptr;

public:
  ffmpeg_encoder() {
    av_register_all();
    avcodec_register_all();
  }
  ~ffmpeg_encoder() { close(); }

  int open(codec fmt, AVCodecContext *decoder_context, stream_type &stream,
           const std::size_t io_buf_len = 1024 * 1024 * 4) {
    close();
    stream_ = &stream;

    encoder = avcodec_find_encoder(codec_id(fmt));
    // encoder = avcodec_find_encoder_by_name("libx264");
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
                                    NULL, write_buffer, NULL);
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

    if (format_context->oformat->flags & AVFMT_GLOBALHEADER)
      codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avformat_write_header(format_context, NULL) < 0) {
      return -5;
    }

    // for test
    /*
        if (flush_encoder(format_context, 0)) {
          return -7;
        }
        if (av_write_trailer(format_context)) {
          return -6;
        }
    */
    return 0;
  }

  int write(AVFrame *frame) {
    frame->pts = av_frame_get_best_effort_timestamp(frame);
    frame->pict_type = AV_PICTURE_TYPE_NONE;

    AVPacket packet;
    packet.data = NULL;
    packet.size = 0;
    av_init_packet(&packet);

    int got_frame;

    if (avcodec_encode_video2(format_context->streams[0]->codec, &packet, frame,
                              &got_frame) < 0) {
      got_frame = 0;
    }
    if (!got_frame) {
      return -1;
    }
    if (av_write_frame(format_context, &packet) < 0) {
      return -2;
    }
    return 0;
  }

  void close() {
    if (format_context) {
      avformat_free_context(format_context);
      format_context = nullptr;
    }
    if (codec_context) {
      avcodec_close(codec_context);
      codec_context = nullptr;
    }
    if (encoder) {
      encoder = nullptr;
    }
    if (io_context) {
      av_free(io_context);
      io_context = nullptr;
    }
    if (av_stream) {
      av_stream = nullptr;
    }
    if (io_context_buf) {
      av_free(io_context_buf);
      io_context_buf = nullptr;
    }
  }

private:
  static int write_buffer(void *opaque, uint8_t *buf, int buf_size) {
    auto *s = (std::basic_ostream<unsigned char> *)opaque;
    s->write((unsigned char *)buf, buf_size);
    return buf_size;
  }

  static int flush_encoder(AVFormatContext *fmt_ctx,
                           unsigned int stream_index) {
    int ret;
    int got_frame;
    AVPacket enc_pkt;
    if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
          AV_CODEC_CAP_DELAY))
      return 0;
    while (1) {
      av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
      // ret = encode_write_frame(NULL, stream_index, &got_frame);
      enc_pkt.data = NULL;
      enc_pkt.size = 0;
      av_init_packet(&enc_pkt);
      ret = avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec,
                                  &enc_pkt, NULL, &got_frame);
      av_frame_free(NULL);
      if (ret < 0)
        break;
      if (!got_frame) {
        ret = 0;
        break;
      }
      /* prepare packet for muxing */
      enc_pkt.stream_index = stream_index;
      enc_pkt.dts = av_rescale_q_rnd(
          enc_pkt.dts, fmt_ctx->streams[stream_index]->codec->time_base,
          fmt_ctx->streams[stream_index]->time_base,
          (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
      enc_pkt.pts = av_rescale_q_rnd(
          enc_pkt.pts, fmt_ctx->streams[stream_index]->codec->time_base,
          fmt_ctx->streams[stream_index]->time_base,
          (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
      enc_pkt.duration = av_rescale_q(
          enc_pkt.duration, fmt_ctx->streams[stream_index]->codec->time_base,
          fmt_ctx->streams[stream_index]->time_base);
      av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
      /* mux encoded frame */
      ret = av_write_frame(fmt_ctx, &enc_pkt);
      if (ret < 0)
        break;
    }
    return ret;
  }
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
