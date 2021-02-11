#ifndef __HCAM_FILE_HELPER_H__
#define __HCAM_FILE_HELPER_H__
#include <string>
#include <vector>

namespace hcam {

bool read_file(const std::string &name, std::vector<uint8_t> &dst);
bool write_file(const std::string &name, const void *data, uint32_t len);
uint64_t file_length(const std::string &name, bool &succ);

} // namespace hcam

#endif