#ifndef __HCAM_CONFIG_H__
#define __HCAM_CONFIG_H__
#include "lazy/lazy.h"
#include "video/codec.h"
#include <lazy/lazy.h>
#include <set>
#include <string>

namespace hcam {

struct config {
  static liuziangexit_lazy::lazy_t<config, std::string> lazy;
  static config get() { return lazy.get_instance(); }
  static decltype(lazy) &get_lazy() { return lazy; }

  enum log_level_t { DEBUG, INFO, WARN, ERROR, FATAL };
  log_level_t
      log_level; //只输出log_level和比它更高的log。比如，log_level==INFO，那么DEBUG不会输出
  std::string log_file;       //日志文件
  std::string log_fopen_mode; //用fopen打开日志文件时候的mode参数
  std::set<std::string> disable_log_module; //不输出log的模块
  uint32_t video_thread_count;              //线程数
  uint32_t web_thread_count;                //线程数
  enum codec cam_pix_fmt;                   //摄像头的像素格式
  uint32_t duration;                        //每个视频文件的持续时间
  std::string save_location;                //保存目录
  enum codec output_codec;                  //视频文件编码格式
  int max_storage;     //输出目录最大占用空间（GB）
  std::string device;  //摄像头设备挂载点
  cv::Size resolution; //分辨率
  int fps;             //帧率
  enum timestamp_pos_t {
    UPPER_RIGHT,
    UPPER_LEFT,
    LOWER_LEFT,
    LOWER_RIGHT,
    CENTER
  };
  timestamp_pos_t
      timestamp_pos; //视频时间戳位置 0-右上 1-左上 2-左下 3-右下 4-中间
  enum display_fps_t { NEVER, WARN_ONLY, ALWAYS };
  display_fps_t display_fps; //帧率显示模式 0-NEVER 1-WARNING ONLY 2-ALWAYS
  int font_height;           //视频时间戳字体大小
  std::string web_addr;      // web服务地址
  int port;                  // web服务端口
  bool ssl_enabled;          // 是否支持ssl
  int ssl_port;              // web服务端口
  std::string ssl_cert;      // ssl证书
  std::string ssl_key;       // ssl私钥
  std::string web_root;      // web目录
  int idle_timeout;          // web服务连接闲置超时时间（秒）

  friend typename liuziangexit_lazy::lazy_t<config, std::string>;

  config() = default;
  config(const config &) = default;
  config(config &&) = default;
  bool read(const std::string &);

private:
  config(const std::string &);

  bool write(const std::string &);
};

} // namespace hcam

#endif
