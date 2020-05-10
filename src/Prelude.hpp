#pragma once

#include "Log.hpp"

#include <string>
#include <utility>

namespace {

template <class... Fs>
struct overload : Fs... {
    template <class... Ts>
    overload(Ts&&... ts) : Fs{std::forward<Ts>(ts)}...
    {
    }

    using Fs::operator()...;
};

template <class... Ts>
overload(Ts&&...)->overload<std::remove_reference_t<Ts>...>;

template <class... Fs>
auto make_visitor(Fs... fs)
{
    return overload<Fs...>(fs...);
}

} // namespace

// TODO: remove this and just do something simpler
#define MATCH(X, ...) std::visit(make_visitor(__VA_ARGS__), X)

inline std::string build_string(const char* cstr)
{
    return cstr ? std::string(cstr) : std::string();
}

template <typename... Args>
void cstr_format(char* buffer, size_t sizeof_buffer, const char* fmt, Args... args)
{
    // TODO: handle errors properly here
#if _WIN32
    const int ret_code = 1 + sprintf_s(buffer, sizeof_buffer, fmt, args...);
#else
    const int ret_code = 1 + snprintf(buffer, sizeof_buffer, fmt, args...);
#endif
    if ((size_t)ret_code < 0) {
        LOG(Warning) << "Encoding error encountered in cstr_format.";
    }
    else if ((size_t)ret_code > sizeof_buffer) {
        LOG(Warning) << "Buffer overflow in cstr_format.";
    }
}
