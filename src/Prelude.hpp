#pragma once

#include <utility>

// #include <experimental/optional>
//
// template <typename T>
// using optional = std::experimental::optional<T>;

// template <typename Container, typename T>
// bool contains(const Container<T>& c, const T& item) {
//     return std::find(std::begin(c), std::end(c), item);
// }
//


template <class ...Fs>
struct overload : Fs... {
  template <class ...Ts>
  overload(Ts&& ...ts) : Fs{std::forward<Ts>(ts)}...
  {}

  using Fs::operator()...;
};

template <class ...Ts>
overload(Ts&&...) -> overload<std::remove_reference_t<Ts>...>;

template <class... Fs>
auto make_visitor(Fs... fs)
{
    return overload<Fs...>(fs...);
}

#define MATCH(X, ...) std::visit(make_visitor(__VA_ARGS__), X)
