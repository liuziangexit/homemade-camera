/**
 * @author  liuziang
 * @contact liuziang@liuziangexit.com
 * @date    2/25/2019
 *
 * lazy
 *
 * A thread-safe wrapper class that provides lazy initialization semantics for
 * any type.
 *
 * Requires C++17.
 *
 * TODO 直接存lazy对象里就好了，不要再用alloctor去分配空间了
 */
#pragma once
#ifndef _liuziangexit_lazy
#define _liuziangexit_lazy
#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace liuziangexit_lazy {

namespace detail {

template <std::size_t...> struct sequence {};

template <std::size_t Size, std::size_t... Sequence>
struct make_integer_sequence
    : make_integer_sequence<Size - 1, Size - 1, Sequence...> {};

template <std::size_t... Sequence>
struct make_integer_sequence<0, Sequence...> {
  using type = sequence<Sequence...>;
};

template <typename Func, typename Tuple, std::size_t... index>
constexpr void do_call(const Func &func, Tuple &&tuple, sequence<index...>) {
  func(std::get<index>(std::forward<Tuple>(tuple))...);
}

template <typename Func, typename Tuple>
constexpr void function_call(const Func &func, Tuple &&tuple) {
  do_call(func, std::forward<Tuple>(tuple),
          typename make_integer_sequence<
              std::tuple_size_v<std::remove_reference_t<Tuple>>>::type());
}

} // namespace detail

template <typename Ty, typename Alloc, typename... ConstructorArgs> class lazy {
public:
  using value_type =
      typename std::remove_reference_t<typename std::remove_cv_t<Ty>>;
  using allocator_type = Alloc;
  using reference = value_type &;
  using const_reference = const value_type &;
  using pointer = value_type *;

  static_assert(
      std::is_same_v<
          value_type, //
          typename std::allocator_traits<allocator_type>::value_type>,
      "lazy::value_type should equals to lazy::allocator_type::value_type");

private:
  using constructor_arguments_tuple = std::tuple<ConstructorArgs...>;

public:
  /*
   Construct lazy object.

   Use _DeductionTrigger(type parameter of templated constructor) rather than
   _ConstructorArgs(type parameter of template class) to trigger a deduction.

   This deduction makes variable 'args' becomes a forwarding reference but not
   a rvalue reference.
  */
  template <typename... DeductionTrigger>
  constexpr explicit lazy(const allocator_type &alloc,
                          DeductionTrigger &&...args)
      : m_instance(nullptr), //
        m_allocator(alloc),  //
        m_constructor_arguments(
            std::make_tuple(std::forward<DeductionTrigger>(args)...)) {}

  lazy(const lazy &) = delete;

  lazy(lazy &&rhs) noexcept
      : m_instance(rhs.m_instance.load(std::memory_order_relaxed)),
        m_allocator(std::move(rhs.m_allocator)),
        m_constructor_arguments(std::move(rhs.m_constructor_arguments)) {
    rhs.m_instance.store(nullptr, std::memory_order_relaxed);
  }

  lazy &operator=(const lazy &) = delete;

  lazy &operator=(lazy &&rhs) noexcept {
    if (this == &rhs)
      return *this;
    this->~lazy();
    return *new (this) lazy(std::move(rhs));
  }

  ~lazy() noexcept {
    auto ins = this->m_instance.load(std::memory_order_relaxed);
    if (ins) {
      ins->~value_type();
    }
    this->m_allocator.deallocate(
        this->m_instance.load(std::memory_order_relaxed), 1);
  }

public:
  /*
   Get the lazily initialized value.

   This function is exception-safe. if the constructor throws an exception,
   that exception WILL rethrow and WILL NOT cause any leak or illegal state.

   If memory allocation fails or constructor throws std::bad_alloc,
   std::bad_alloc will be thrown.
  */
  value_type &get_instance() {
    value_type *instance =
        m_instance.load(std::memory_order::memory_order_acquire);
    if (!instance) {
      std::lock_guard<std::mutex> guard(m_lock);
      instance = m_instance.load(std::memory_order::memory_order_relaxed);
      if (!instance) {
        // allocate memory
        instance = this->m_allocator.allocate(1);
        try {
          // invoke constructor
          detail::function_call(
              [instance](auto &&...args) {
                new (instance) value_type( //
                    std::forward<decltype(args)>(args)...);
              },
              std::move(this->m_constructor_arguments));
        } catch (...) {
          this->m_allocator.deallocate(instance, 1);
          std::rethrow_exception(std::current_exception());
        }
        m_instance.store(instance, std::memory_order::memory_order_release);
      }
    }
    return *instance;
  }

  // Indicates whether a value has been created.
  bool is_instance_created() {
    return m_instance.load(std::memory_order::memory_order_acquire);
  }

  bool set_instance(value_type &&val) {
    return emplace_instance(std::move(val));
  }

  bool set_instance(value_type &val) { return emplace_instance(val); }

  template <typename... ARGS> bool emplace_instance(ARGS &&...args) {
    bool first_time;
    std::lock_guard<std::mutex> guard(m_lock);
    value_type *instance =
        m_instance.load(std::memory_order::memory_order_relaxed);
    if (!instance) {
      // allocate memory
      instance = this->m_allocator.allocate(1);
      first_time = true;
    } else {
      instance->~value_type();
      first_time = false;
    }
    instance = new (instance) value_type(std::forward<ARGS>(args)...);
    m_instance.store(instance, std::memory_order::memory_order_release);
    return first_time;
  }

private:
  std::atomic<pointer> m_instance;
  allocator_type m_allocator;
  constructor_arguments_tuple m_constructor_arguments;
  std::mutex m_lock;
};

template <typename Ty, typename... ConstructorArgs>
auto make_lazy(ConstructorArgs &&...constructor_args) {
  return lazy<Ty, std::allocator<Ty>,
              std::remove_reference_t<std::remove_cv_t<ConstructorArgs>>...>(
      std::allocator<Ty>(), std::forward<ConstructorArgs>(constructor_args)...);
}

template <typename Ty, typename... ConstructorArgs>
using lazy_t =
    lazy<Ty, std::allocator<Ty>,
         std::remove_reference_t<std::remove_cv_t<ConstructorArgs>>...>;

} // namespace liuziangexit_lazy
#endif
