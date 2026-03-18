//
// Created by LMR on 2026/3/18.
// from:
// https://github.com/Awesome-Embedded-Learning-Studio/Tutorial_AwesomeModernCPP/blob/main/codes_and_assets/examples/chapter08/05_expected/expected.hpp
//
// other project: https://github.com/nonstd-lite/expected-lite

#pragma once

#include <stdexcept>
#include <type_traits>
#include <utility>

// Simplified std::expected implementation
// example:
// expected<int, std::string> parse_int(const std::string& s) {
//   try {
//     size_t pos;
//     int v = std::stoi(s, &pos);
//     if (pos != s.size())
//        return unexpected<std::string>{"trailing chars"};
//     return expected<int, std::string>(v);
//   } catch (...) {
//     return unexpected<std::string>{"not a number"};
//   }
// }
//
// int main() {
//   auto r = parse_int("123");
//   if (r.has_value())
//     std::cout << "value=" << r.value() << "\n";
//   else
//     std::cerr << "error: " << r.error() << "\n";
//
//   // 链式
//   auto final = parse_int("42").and_then([](int x){
//       return expected<double, std::string>(x / 2.0);
//   }).map([](double d){ return d * 3.0; });
// }

// 用于构造错误分支的辅助类型
template<typename E>
struct unexpected {
  E value;
};

template<typename T, typename E>
class expected {
  bool has_value_;
  union {
    T val_;
    E err_;
  } storage_;

public:
  expected(const T& v) : has_value_(true) {
    // placement new, grammar: new (address) Type(initializer)
    new (&storage_.val_) T(v);
  }
  expected(T&& v) : has_value_(true) { new (&storage_.val_) T(std::move(v)); }

  // 构造错误
  expected(unexpected<E> u) : has_value_(false) { new (&storage_.err_) E(std::move(u.value)); }

  ~expected() {
    if (has_value_)
      storage_.val_.~T();
    else
      storage_.err_.~E();
  }

  bool has_value() const noexcept { return has_value_; }

  T& value() {
    if (!has_value_)
      throw std::runtime_error("bad expected access");
    return storage_.val_;
  }

  const E& error() const {
    if (has_value_)
      throw std::runtime_error("no error present");
    return storage_.err_;
  }

  T value_or(T default_value) const {
    if (has_value_)
      return storage_.val_;
    return default_value;
  }

  // 将成功值用函数 f 转换为另一个 expected
  template<typename F>
  auto map(F f) const -> expected<decltype(f(std::declval<T>())), E> {
    using U = decltype(f(std::declval<T>()));
    if (has_value_)
      return expected<U, E>(f(storage_.val_));
    return expected<U, E>(unexpected<E>{storage_.err_});
  }

  // 链式调用，f 返回 expected<U, E>
  template<typename F>
  auto and_then(F f) const -> decltype(f(std::declval<T>())) {
    if (has_value_)
      return f(storage_.val_);
    return decltype(f(std::declval<T>()))(unexpected<E>{storage_.err_});
  }
};