#ifndef __HCAM_CONFIG_H__
#define __HCAM_CONFIG_H__
#include "video/codec.h"
#include <string>

namespace hcam {

class config {
public:
  //这俩暂时没用到，现在直接改opencv里的默认值
  cv::Size resolution;
  int fps;

  enum codec cam_pix_fmt;    //摄像头的像素格式
  uint32_t duration;         //每个视频文件的持续时间
  std::string save_location; //保存目录
  enum codec output_codec;   //视频文件编码格式
  std::string device;        //摄像头设备挂载点
  int timestamp_pos; //视频时间戳位置 0-右上 1-左上 2-左下 3-右下 4-中间
  int display_fps; //帧率显示模式 0-NEVER 2-WARNING ONLY 3-ALWAYS
  int font_height; //视频时间戳字体大小
  int web_port;    // web服务端口
  int tcp_timeout; // web服务tcp超时时间（秒）

  config(const std::string &);

  bool read(const std::string &);

  bool write(const std::string &);
};

} // namespace hcam

#endif