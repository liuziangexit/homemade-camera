#include "util/time_util.h"
#include <stdexcept>
#include <stdint.h>
#include <time.h>

namespace hcam {

//得到一个单调的时间
//指定精度(分母是10的多少次方)
//比如power=3就是毫秒（1/1000）
//最大power=9，纳秒
//最小power=0，秒
uint64_t checkpoint(uint32_t power) {
  if (power > 9)
    throw std::invalid_argument("");
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  auto target = pow(ts.tv_sec, power);
  auto delta = pow(ts.tv_nsec, power - 9);
  return target + delta;
}

} // namespace hcam
