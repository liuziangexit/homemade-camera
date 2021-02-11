#include "util/file_helper.h"
#include <memory>
#include <stdio.h>

namespace hcam {

bool read_file(const std::string &name, std::vector<uint8_t> &dst) {
  std::unique_ptr<FILE, void (*)(FILE *)> fp(fopen(name.c_str(), "rb"),
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
  dst.resize(size);
  auto actual_read = fread(dst.data(), 1, size, fp.get());
  return (uint64_t)size == (uint64_t)actual_read;
}

bool write_file(const std::string &name, const void *data, uint32_t len) {
  std::unique_ptr<FILE, void (*)(FILE *)> fp(fopen(name.c_str(), "wb"),
                                             [](FILE *fp) {
                                               if (fp)
                                                 fclose(fp);
                                             });
  if (!fp)
    return false;
  auto actual_write = fwrite(data, 1, len, fp.get());
  return actual_write == len;
}

// TODO 只读inode就够了
uint64_t file_length(const std::string &name, bool &succ) {
  std::unique_ptr<FILE, void (*)(FILE *)> fp(fopen(name.c_str(), "rb"),
                                             [](FILE *fp) {
                                               if (fp)
                                                 fclose(fp);
                                             });
  if (!fp) {
    succ = false;
    return 0;
  }
  fseek(fp.get(), 0L, SEEK_END);
  auto size = ftell(fp.get());
  if (size == -1L) {
    succ = false;
    return 0;
  }
  succ = true;
  return size;
}

} // namespace hcam
