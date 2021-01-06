#ifndef __HOMEMADECAM_GUARD_H__
#define __HOMEMADECAM_GUARD_H__
#include <functional>
#include <type_traits>
#include <utility>

namespace homemadecam {

class guard {
public:
  template <typename F> guard(F f) : fun(std::move(f)) {
  }

  ~guard() { fun(); }

private:
  const std::function<void()> fun;
};

} // namespace homemadecam

#endif