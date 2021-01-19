#ifndef __HCAM_TIME_UTIL_H__
#define __HCAM_TIME_UTIL_H__
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace hcam {

template <typename T> T pow(T num, int32_t pow) {
  for (int i = 0, count = pow > 0 ? pow : -pow; i < count; i++) {
    if (pow >= 0) {
      num *= 10;
    } else {
      num /= 10;
    }
  }
  return num;
}

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

#endif