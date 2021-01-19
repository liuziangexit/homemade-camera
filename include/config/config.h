#ifndef __HCAM_CONFIG_H__
#define __HCAM_CONFIG_H__
#include "util/logger.h"
#include "video/codec.h"
#include "json/json.hpp"
#include <cstdint>
#include <memory>
#include <opencv2/core/types.hpp>
#include <stdexcept>
#include <string>

namespace hcam {

class config {
public:
  //这俩暂时没用到，现在直接改opencv里的默认值
  cv::Size resolution;
  int fps;

  enum codec palette;        //摄像头的像素格式
  uint32_t duration;         //每个视频文件的持续时间
  std::string save_location; //保存目录
  enum codec codec;          //视频文件编码格式
  std::string device;        //摄像头设备挂载点
  int text_pos; //视频时间戳位置 0-右上 1-左上 2-左下 3-右下 4-中间
  int font_height; //视频时间戳字体大小
  int web_port;    // web服务端口
  int tcp_timeout; // web服务tcp超时时间（秒）

  config(const std::string &filename) {
    if (!read(filename))
      throw std::runtime_error("read config failed");
  }

  bool read(const std::string &filename) {
    std::unique_ptr<FILE, void (*)(FILE *)> fp(fopen(filename.c_str(), "rb"),
                                               [](FILE *fp) {
                                                 if (fp)
                                                   fclose(fp);
                                               });
    if (!fp)
      return false;
    fseek(fp.get(), 0L, SEEK_END);
    auto size = ftell(fp.get());
    if (size == -1L)
      return false;
    rewind(fp.get());
    std::string raw((std::size_t)size, 0);
    auto actual_read = fread(const_cast<char *>(raw.data()), 1, size, fp.get());
    if ((uint64_t)size != (uint64_t)actual_read)
      return false;
    using namespace nlohmann;
    json js;
    try {
      js = json::parse(raw);

      this->palette = codec_parse(js["palette"].get<std::string>());
      this->duration = js["duration"].get<uint32_t>();
      this->save_location = js["save-location"].get<std::string>();
      this->codec = codec_parse(js["codec"].get<std::string>());
      this->device = js["device"].get<std::string>();
      {
        std::string r = js["resolution"].get<std::string>();
        auto pos = r.find('x');
        if (pos == std::string::npos)
          return false;
        try {
          int width = std::stoi(r.substr(0, pos));
          int height = std::stoi(r.substr(pos + 1, r.length()));
          this->resolution = cv::Size{width, height};
        } catch (const std::exception &) {
          return false;
        }
      }
      this->fps = js["fps"].get<int>();
      this->text_pos = js["text-pos"].get<int>();
      this->font_height = js["font-height"].get<int>();
      this->web_port = js["web-port"].get<int>();
      this->tcp_timeout = js["tcp-timeout"].get<int>();
    } catch (const std::exception &ex) {
      logger::error(ex.what());
      return false;
    }
    return true;
  }

  bool write(const std::string &filename) {
    std::unique_ptr<FILE, void (*)(FILE *)> fp(fopen(filename.c_str(), "wb"),
                                               [](FILE *fp) {
                                                 if (fp)
                                                   fclose(fp);
                                               });
    if (!fp)
      return false;

    throw std::runtime_error("not implemented!!");

    using namespace nlohmann;
    json js = {
        {"duration", this->duration},
        {"save-location", this->save_location},
        {"codec", codec_to_string(this->codec)},
        {"device", this->device},
        {"text-pos", this->text_pos},
        {"font-height", this->font_height},
        {"web-port", this->web_port},
        {"tcp-timeout", this->tcp_timeout} //
    };

    std::string raw = js.dump(4);
    auto actual_write = fwrite(raw.data(), 1, raw.size(), fp.get());
    if (actual_write != raw.size())
      return false;
    return true;
  }
};

} // namespace hcam

#endif