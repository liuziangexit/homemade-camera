#ifndef __HOMEMADECAM_CONFIG_H__
#define __HOMEMADECAM_CONFIG_H__
#include "codec.h"
#include "logger.h"
#include "json/json.hpp"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <string>

namespace homemadecam {

class config {
public:
  uint32_t duration;
  std::string save_location;
  codec codec;
  int camera_id;

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
    if (!fp.get())
      return false;
    fseek(fp.get(), 0L, SEEK_END);
    auto size = ftell(fp.get());
    if (size == -1L)
      return false;
    rewind(fp.get());
    std::string raw((std::size_t)size, 0);
    auto actual_read = fread(const_cast<char *>(raw.data()), 1, size, fp.get());
    if (size != actual_read)
      return false;
    using namespace nlohmann;
    json js;
    try {
      js = json::parse(raw);

      this->duration = js["duration"].get<uint32_t>();
      this->save_location = js["save-location"].get<std::string>();
      this->codec = codec_parse(js["codec"].get<std::string>());
      this->camera_id = js["camera-id"].get<int>();
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

    using namespace nlohmann;
    json js = {
        {"duration", this->duration},
        {"save-location", this->save_location},
        {"codec", codec_to_string(this->codec)},
        {"camera-id", this->camera_id} //
    };

    std::string raw = js.dump(4);
    auto actual_write = fwrite(raw.data(), 1, raw.size(), fp.get());
    if (actual_write != raw.size())
      return false;
    return true;
  }
};

} // namespace homemadecam

#endif