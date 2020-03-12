#pragma once

#include <utility>
#include <string>

namespace {

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

}

#define MATCH(X, ...) std::visit(make_visitor(__VA_ARGS__), X)

inline std::string build_string(const char* cstr) {
    return cstr ? std::string(cstr) : std::string();
};
