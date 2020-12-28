#ifndef __HOMECAM_FFMPEG_CAPTURE_H_
#define __HOMECAM_FFMPEG_CAPTURE_H_

namespace homemadecam {

class ffmpeg_capture {
public:
  void list_devices();
  int open_device();
  void grab_frame();
  int close_device();
};

} // namespace homemadecam

#endif // HOMECAM_WEB_SERVICE_H
