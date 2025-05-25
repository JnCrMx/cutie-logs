module;
#include <charconv>
#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <ranges>
#include <string_view>
#include <string>
#include <type_traits>

export module common:stencil_functions;

import glaze;
import :mmdb;
import :utils;
import :structs;

namespace common {

uint32_t parse_ipv4(std::string_view x) {
    uint32_t result = 0;
    int count = 0;
    for(auto& c : x) {
        if(c == '.') {
            ++count;
        }
    }
    if(count != 3) {
        return 0;
    }
    std::string_view s = x;
    for(int i = 0; i < 4; ++i) {
        auto pos = s.find('.');
        if(pos == std::string_view::npos) {
            pos = s.size();
        }
        auto part = s.substr(0, pos);
        int part_int = parse_int(part).value_or(0);
        result = (result << 8) | (part_int & 0xFF);
        s.remove_prefix(pos + 1);
    }
    return result;
}

glz::json_t get_path(glz::json_t x, std::string_view path) {
    if(path.empty()) {
        return x;
    }
    glz::json_t y;
    for(auto part : path | std::views::split('.')) {
        std::string_view sv{part};
        if(x.is_array()) {
            int index = parse_int(sv).value_or(0);
            if(index < 0) {
                index = x.size() + index;
            }
            if(index < 0 || index >= x.size()) {
                return glz::json_t{};
            }
            y = x.get_array()[index];
        } else if(x.is_object()) {
            if(!x.contains(sv)) {
                return glz::json_t{};
            }
            y = x[sv];
        } else {
            return glz::json_t{};
        }
        x = y;
    }
    return x;
}

template<typename T>
concept log_with_resource = requires(T t) {
    { t.resource } -> std::convertible_to<const log_resource*>;
    { t.log } -> std::convertible_to<const log_entry*>;
};

export struct stencil_functions {
    std::add_pointer_t<double(glz::json_t)> get_number = [](glz::json_t x){
        if(!x.is_number()) {
            return 0.0;
        }
        return x.get_number();
    };
    std::add_pointer_t<std::string(glz::json_t)> get_string = [](glz::json_t x){
        if(!x.is_string()) {
            return std::string{};
        }
        return x.get_string();
    };
    std::add_pointer_t<bool(glz::json_t)> get_boolean = [](glz::json_t x){
        if(!x.is_boolean()) {
            return false;
        }
        return x.get_boolean();
    };

    std::add_pointer_t<glz::json_t(glz::json_t, std::string_view)> at = [](glz::json_t x, std::string_view arg){
        if(x.is_array()) {
            int index = parse_int(arg).value_or(0);
            if(index < 0) {
                index = x.size() + index;
            }
            if(index < 0 || index >= x.size()) {
                return glz::json_t{};
            }
            return x.get_array()[index];
        } else if(x.is_object()) {
            if(!x.contains(arg)) {
                return glz::json_t{};
            }
            return x[arg];
        } else {
            return glz::json_t{};
        }
    };
    std::add_pointer_t<glz::json_t(glz::json_t, std::string_view)> get = [](glz::json_t x, std::string_view arg){
        return get_path(x, arg);
    };

    std::add_pointer_t<double(double, std::string_view)> add = [](double x, std::string_view y){
        return x + parse_int(y).value_or(0);
    };
    std::add_pointer_t<double(double, std::string_view)> sub = [](double x, std::string_view y){
        return x - parse_int(y).value_or(0);
    };
    std::add_pointer_t<double(double, std::string_view)> mul = [](double x, std::string_view y){
        return x * parse_int(y).value_or(0);
    };
    std::add_pointer_t<double(double, std::string_view)> div = [](double x, std::string_view y){
        return x / parse_int(y).value_or(1);
    };
    std::add_pointer_t<double(double, std::string_view)> mod = [](double x, std::string_view y){
        return std::fmod(x, parse_int(y).value_or(1));
    };

    struct {
        std::string operator()(std::string_view x) const {
            return std::string{x};
        }
        std::string operator()(double x) const {
            return std::to_string(x);
        }
        std::string operator()(bool x) const {
            return x ? "true" : "false";
        }
        std::string operator()(const glz::json_t& x) const {
            return x.dump().value_or("null");
        }
        std::string operator()(uint32_t x) const {
            return std::to_string(x);
        }
    } to_string{};
    std::add_pointer_t<std::string(std::string)> to_upper = [](std::string x){
        for(auto& c : x) {
            c = std::toupper(c);
        }
        return x;
    };
    std::add_pointer_t<std::string(std::string)> to_lower = [](std::string x){
        for(auto& c : x) {
            c = std::tolower(c);
        }
        return x;
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> pad_left = [](std::string x, std::string_view y){
        int n = parse_int(y).value_or(0);
        return std::format("{:>{}}", x, n);
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> pad_right = [](std::string x, std::string_view y){
        int n = parse_int(y).value_or(0);
        return std::format("{:<{}}", x, n);
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> pad_both = [](std::string x, std::string_view y){
        int n = parse_int(y).value_or(0);
        return std::format("{:^{}}", x, n);
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> truncate = [](std::string x, std::string_view y){
        int n = parse_int(y).value_or(0);
        if(x.size() > n) {
            return x.substr(0, n);
        }
        return x;
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> truncate_left = [](std::string x, std::string_view y){
        int n = parse_int(y).value_or(0);
        if(x.size() > n) {
            return x.substr(x.size() - n);
        }
        return x;
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> repeat = [](std::string x, std::string_view y){
        int n = parse_int(y).value_or(0);
        std::string result;
        for(int i = 0; i < n; ++i) {
            result += x;
        }
        return result;
    };
    std::add_pointer_t<std::string(std::string)> reverse = [](std::string x){
        std::reverse(x.begin(), x.end());
        return x;
    };

    using sys_seconds_double = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>;
    std::add_pointer_t<sys_seconds_double(double)> from_timestamp = [](double x){
        return sys_seconds_double{std::chrono::duration<double>{x}};
    };
    std::add_pointer_t<double(sys_seconds_double)> to_timestamp = [](sys_seconds_double x){
        return x.time_since_epoch().count();
    };
    std::add_pointer_t<std::string(sys_seconds_double)> iso_date_time = [](sys_seconds_double x){
        return std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::time_point_cast<std::chrono::seconds>(x));
    };
    std::add_pointer_t<std::string(sys_seconds_double)> iso_date = [](sys_seconds_double x){
        return std::format("{:%Y-%m-%d}", std::chrono::time_point_cast<std::chrono::seconds>(x));
    };
    std::add_pointer_t<std::string(sys_seconds_double)> iso_time = [](sys_seconds_double x){
        return std::format("{:%H:%M:%S}", std::chrono::time_point_cast<std::chrono::seconds>(x));
    };
    std::add_pointer_t<std::string(sys_seconds_double)> iso_8601 = [](sys_seconds_double x){
        return std::format("{:%Y-%m-%dT%H:%M:%S.000Z}", std::chrono::time_point_cast<std::chrono::seconds>(x));
    };
    std::add_pointer_t<std::string(sys_seconds_double)> strftime = [](sys_seconds_double x){
        return std::format("{:%d/%b/%Y:%H:%M:%S %z}", std::chrono::time_point_cast<std::chrono::seconds>(x));
    };

    std::add_pointer_t<uint32_t(std::string)> parse_ipv4 = [](std::string x) -> uint32_t {
        return ::common::parse_ipv4(x);
    };

    struct {
        std::string operator()(const log_with_resource auto& o) const {
            return o.resource->guess_name().value_or(std::format("Resource #{}", o.log->resource));
        }
        std::string operator()(const log_resource& r) const {
            return r.guess_name().value_or("Unknown Resource");
        }
    } resource_name;

    std::add_pointer_t<uint32_t(common::log_severity)> severity_color = [](common::log_severity severity) -> uint32_t {
        switch(severity) {
            case common::log_severity::UNSPECIFIED:
                return 0xFFFFFF; // White for unspecified
            case common::log_severity::TRACE:
            case common::log_severity::TRACE2:
            case common::log_severity::TRACE3:
            case common::log_severity::TRACE4:
                return 0xCCCCCC; // Light gray for trace
            case common::log_severity::DEBUG:
            case common::log_severity::DEBUG2:
            case common::log_severity::DEBUG3:
            case common::log_severity::DEBUG4:
                return 0x0000FF; // Blue for debug
            case common::log_severity::INFO:
            case common::log_severity::INFO2:
            case common::log_severity::INFO3:
            case common::log_severity::INFO4:
                return 0x00FF00; // Green for info
            case common::log_severity::WARN:
            case common::log_severity::WARN2:
            case common::log_severity::WARN3:
            case common::log_severity::WARN4:
                return 0xFFFF00; // Yellow for warning
            case common::log_severity::ERROR:
            case common::log_severity::ERROR2:
            case common::log_severity::ERROR3:
            case common::log_severity::ERROR4:
                return 0xFF0000; // Red for error
            case common::log_severity::FATAL:
            case common::log_severity::FATAL2:
            case common::log_severity::FATAL3:
            case common::log_severity::FATAL4:
                return 0xCC0000; // Dark red for fatal
            default:
                return 0xFFFFFF; // White for unknown severity
        }
    };
    std::add_pointer_t<std::string(uint32_t)> hex_color = [](uint32_t color) {
        return std::format("#{0:06X}", color);
    };
};

export struct advanced_stencil_functions {
    std::vector<std::pair<std::string_view, mmdb*>> m_mmdbs;

    stencil_functions base{};

    struct {
        std::vector<std::pair<std::string_view, mmdb*>> m_mmdbs;

        glz::json_t operator()(uint32_t x) const {
            glz::json_t result = glz::json_t::object_t{};
            for(auto& [mmdb_name, mmdb] : m_mmdbs) {
                auto res = mmdb->lookup_v4(x);
                if(res) {
                    result[mmdb_name] = res->to_json();
                }
            }
            return result;
        }
        glz::json_t operator()(__uint128_t x) const {
            glz::json_t result = glz::json_t::object_t{};
            for(auto& [mmdb_name, mmdb] : m_mmdbs) {
                auto res = mmdb->lookup_v6(x);
                if(res) {
                    result[mmdb_name] = res->to_json();
                }
            }
            return result;
        }
        glz::json_t operator()(std::string x) const {
            if(x.contains(":")) {
                return glz::json_t{};
            } else if(x.contains(".")) {
                return (*this)(parse_ipv4(x));
            }
            return glz::json_t{};
        }
    } lookup{m_mmdbs};
};

}
