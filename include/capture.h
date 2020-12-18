#ifndef __HOMEMADECAM_CAPTURE_H__
#define __HOMEMADECAM_CAPTURE_H__
#include <string>

namespace homemadecam {
enum codec { H264, H265 };
void capture_begin(const std::string &save_directory, codec codec,
                   uint32_t duration);
void capture_end();

} // namespace homemadecam

#endif