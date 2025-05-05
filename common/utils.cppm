module;
#include <charconv>
#include <chrono>
#include <optional>
#include <string_view>
#include <glaze/util/fast_float.hpp>

export module common:utils;

namespace common {

export std::string format_duration(std::chrono::seconds duration) {
    if(duration <= std::chrono::seconds{0}) {
        return "0 seconds";
    }
    if(duration < std::chrono::minutes{1}) {
        return std::format("{} seconds", duration.count());
    }
    if(duration < std::chrono::hours{1}) {
        return std::format("{:.2f} minutes", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::minutes::period>>(duration).count());
    }
    if(duration < std::chrono::hours{24}) {
        return std::format("{:.2f} hours", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::hours::period>>(duration).count());
    }
    return std::format("{:.2f} days", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::hours::period>>(duration).count() / 24);
}

export std::optional<int> parse_int(std::string_view x) {
    int i;
    if(std::from_chars(x.data(), x.data() + x.size(), i).ec != std::errc{}) {
        return std::nullopt;
    }
    return i;
}

export std::optional<double> parse_double(std::string_view x) {
    double d;
    if(glz::fast_float::from_chars(x.data(), x.data() + x.size(), d).ec != std::errc{}) {
        return std::nullopt;
    }
    return d;
}

}
