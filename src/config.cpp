#include "util/logger.h"
#include "video/codec.h"
#include "json/json.hpp"
#include <config/config.h>
#include <cstdint>
#include <memory>
#include <opencv2/core/types.hpp>
#include <stdexcept>
#include <string>

namespace hcam {

liuziangexit_lazy::lazy_t<config, std::string> config::lazy =
    liuziangexit_lazy::make_lazy<config, std::string>("config.json");

config::config(const std::string &filename) {
  if (!read(filename))
    throw std::runtime_error("read config failed");
}

bool config::read(const std::string &filename) {
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

    this->log_level = (log_level_t)js["log-level"].get<int>();
    this->disable_log_module =
        js["disable-log-module"].get<std::set<std::string>>();
    this->video_thread_count = js["video-thread-count"].get<int>();
    this->web_thread_count = js["web-thread-count"].get<int>();
    this->cam_pix_fmt =
        codec_parse(js["camera-pixel-format"].get<std::string>());
    this->duration = js["duration"].get<uint32_t>();
    this->save_location = js["save-location"].get<std::string>();
    this->max_storage = js["max-storage"].get<int>();
    this->output_codec = codec_parse(js["output-codec"].get<std::string>());
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
    this->timestamp_pos = (timestamp_pos_t)js["timestamp-pos"].get<int>();
    this->display_fps = (display_fps_t)js["display-fps"].get<int>();
    this->font_height = js["font-height"].get<int>();
    this->web_addr = js["web-addr"].get<std::string>();
    this->port = js["port"].get<int>();
    this->ssl_enabled = js["ssl-enabled"].get<bool>();
    this->ssl_port = js["ssl-port"].get<int>();
    this->ssl_cert = js["ssl-cert"].get<std::string>();
    this->ssl_key = js["ssl-key"].get<std::string>();
    this->web_root = js["web-root"].get<std::string>();
    this->idle_timeout = js["idle-timeout"].get<int>();
  } catch (const std::exception &ex) {
    //这里不能用logger，因为logger也依赖lazy
    std::cout << "config read failed: " << ex.what() << std::endl;
    return false;
  }
  return true;
}

bool config::write(const std::string &filename) {
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
      {"codec", codec_to_string(this->output_codec)},
      {"device", this->device},
      {"timestamp_pos", this->timestamp_pos},
      {"font-height", this->font_height},
      {"idle-timeout", this->idle_timeout} //
  };

  std::string raw = js.dump(4);
  auto actual_write = fwrite(raw.data(), 1, raw.size(), fp.get());
  if (actual_write != raw.size())
    return false;
  return true;
}

} // namespace hcam
