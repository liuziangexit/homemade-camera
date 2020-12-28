#ifndef __HOMECAM_FFMPEG_ENCODER_H_
#define __HOMECAM_FFMPEG_ENCODER_H_

namespace homemadecam {

class ffmpeg_encoder {
public:
  int open_stream(); // open一个ios stream吧！
  int write_frame();
  void close_stream();
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
