#ifndef __HCAM_GUARD_H__
#define __HCAM_GUARD_H__
#include <functional>
#include <type_traits>
#include <utility>

namespace hcam {

class guard {
public:
  template <typename F> guard(F f) : fun(std::move(f)) {}

  ~guard() { fun(); }

private:
  const std::function<void()> fun;
};

} // namespace hcam

#endif