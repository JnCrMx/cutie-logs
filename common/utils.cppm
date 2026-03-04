module;
#include <bit>
#include <charconv>
#include <chrono>
#include <expected>
#include <optional>
#include <string_view>
#include <glaze/util/fast_float.hpp>

export module common:utils;

namespace common {

export template<std::integral T>
constexpr std::optional<T> from_chars(std::string_view sv, int base = 10, bool strict = true) {
    T value{};
    std::from_chars_result res = std::from_chars(sv.data(), sv.data() + sv.size(), value, base);
    if(res.ec != std::errc{}) {
        return std::nullopt;
    }
    if(strict && res.ptr != (sv.data() + sv.size())) {
        return std::nullopt;
    }
    return value;
}

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
    return from_chars<int>(x, 10, false);
}

export std::optional<double> parse_double(std::string_view x) {
    double d;
    if(glz::fast_float::from_chars(x.data(), x.data() + x.size(), d).ec != std::errc{}) {
        return std::nullopt;
    }
    return d;
}


enum class ip_parse_error {
    empty_string,
    invalid_numeral,
    numeral_too_big,
    bad_format,
};

export std::expected<uint32_t, ip_parse_error> parse_ipv4(std::string_view sv) {
    if(sv.empty()) {
        return std::unexpected(ip_parse_error::empty_string);
    }

    if((sv.starts_with("0x") || sv.starts_with("0X")) && sv.find_first_not_of("xX0123456789ABCEDFabcdef") == std::string_view::npos) {
        sv.remove_prefix(2);
        if(auto v = from_chars<uint32_t>(sv, 16)) {
            return *v;
        } else {
            return std::unexpected(ip_parse_error::invalid_numeral);
        }
    } else if(sv.find_first_not_of("0123456789") == std::string_view::npos) {
        if(auto v = from_chars<uint32_t>(sv, 10)) {
            return *v;
        } else {
            return std::unexpected(ip_parse_error::invalid_numeral);
        }
    }

    uint32_t result{};
    for(int i=0; i<3; i++) { // first (ip to three) numerals fill up from the left and MUST be 8 bit
        auto dot = sv.find('.');
        if(dot == std::string_view::npos) break;

        auto number = sv.substr(0, dot);
        std::optional<uint8_t> v{};
        if(number.starts_with("0x") || number.starts_with("0X")) {
            number.remove_prefix(2);
            v = from_chars<uint8_t>(number, 16);
        } else {
            v = from_chars<uint8_t>(number, 10);
        }

        if(!v) {
            return std::unexpected(ip_parse_error::invalid_numeral);
        }

        result |= (static_cast<uint32_t>(*v) << ((3-i)*8));
        sv = sv.substr(dot+1);
    }

    { // last numeral fills up from the right and MAY be bigger than 8 bit
        std::optional<uint32_t> v{};
        if(sv.starts_with("0x") || sv.starts_with("0X")) {
            sv.remove_prefix(2);
            v = from_chars<uint32_t>(sv, 16);
        } else {
            v = from_chars<uint32_t>(sv, 10);
        }
        if(!v) {
            return std::unexpected(ip_parse_error::invalid_numeral);
        }
        if(*v & result) {
            return std::unexpected(ip_parse_error::numeral_too_big);
        }
        result |= *v;
    }

    return result;
}
export std::expected<__uint128_t, ip_parse_error> parse_ipv6(std::string_view sv) {
    if(sv.empty()) {
        return std::unexpected(ip_parse_error::empty_string);
    }
    if(sv.starts_with(":")) {
        sv.remove_prefix(1);
    }
    __uint128_t left_part{};
    int i;
    bool double_colon = false;
    for(i=7; i>=0; i--) {
        auto colon = sv.find(':');
        if(colon == std::string_view::npos) {
            break;
        }

        auto number = sv.substr(0, colon);

        if(number.empty()) {
            sv = sv.substr(colon+1);
            double_colon = true;
            break;
        }

        auto v = from_chars<uint16_t>(number, 16);
        if(!v) {
            return std::unexpected(ip_parse_error::invalid_numeral);
        }
        left_part |= (static_cast<__uint128_t>(*v) << (16*i));

        sv = sv.substr(colon+1);
    }

    if(!double_colon && i != 0) {
        return std::unexpected(ip_parse_error::bad_format);
    }
    if(sv.empty()) {
        return left_part;
    }

    __uint128_t right_part{};
    for(int j=0; j<i+1; j++) {
        auto colon = sv.find(':');
        if(colon == std::string_view::npos) {
            colon = sv.size();
        }
        auto number = sv.substr(0, colon);

        auto v = from_chars<uint16_t>(number, 16);
        if(!v) {
            return std::unexpected(ip_parse_error::invalid_numeral);
        }
        right_part = ((right_part << 16) | *v);

        if(colon >= sv.size()) {
            break;
        }
        sv = sv.substr(colon+1);
    }
    return left_part | right_part;
}

export __uint128_t ntoh128(__uint128_t v) {
    if constexpr (std::endian::native == std::endian::big) {
        return v;
    } else {
        return std::byteswap(v);
    }
}

export std::string ipv4_to_string(uint32_t ip) {
    return std::format("{}.{}.{}.{}", ip >> 24 & 0xFF, ip >> 16 & 0xFF, ip >> 8 & 0xFF, ip & 0xFF);
}

}
