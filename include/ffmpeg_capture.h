#ifndef __HOMECAM_FFMPEG_CAPTURE_H_
#define __HOMECAM_FFMPEG_CAPTURE_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif
#include <opencv2/core/types.hpp>
#include <sstream>
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
    return 0;
  }
  void grab_frame();
  void close_device() {
    avformat_close_input(&format_context);
    avformat_free_context(format_context);
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
