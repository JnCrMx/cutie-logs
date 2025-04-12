module;
#include <charconv>
#include <chrono>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>

export module common:stencil_functions;

import glaze;

namespace common {

double parse_int(std::string_view x) {
    int i;
    if(std::from_chars(x.data(), x.data() + x.size(), i).ec != std::errc{}) {
        return 0;
    }
    return i;
}

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
    std::add_pointer_t<glz::json_t(glz::json_t, std::string_view)> get_array_element = [](glz::json_t x, std::string_view index){
        if(!x.is_array()) {
            return glz::json_t{};
        }
        int i = parse_int(index);
        if(i < 0 || i >= x.size()) {
            return glz::json_t{};
        }
        return x.get_array()[i];
    };
    std::add_pointer_t<glz::json_t(glz::json_t, std::string_view)> get_object_element = [](glz::json_t x, std::string_view key){
        if(!x.is_object()) {
            return glz::json_t{};
        }
        if(!x.contains(key)) {
            return glz::json_t{};
        }
        return x[key];
    };

    std::add_pointer_t<double(double, std::string_view)> add = [](double x, std::string_view y){
        return x + parse_int(y);
    };
    std::add_pointer_t<double(double, std::string_view)> sub = [](double x, std::string_view y){
        return x - parse_int(y);
    };
    std::add_pointer_t<double(double, std::string_view)> mul = [](double x, std::string_view y){
        return x * parse_int(y);
    };
    std::add_pointer_t<double(double, std::string_view)> div = [](double x, std::string_view y){
        return x / parse_int(y);
    };
    std::add_pointer_t<double(double, std::string_view)> mod = [](double x, std::string_view y){
        return std::fmod(x, parse_int(y));
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
        int n = parse_int(y);
        return std::format("{:>{}}", x, n);
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> pad_right = [](std::string x, std::string_view y){
        int n = parse_int(y);
        return std::format("{:<{}}", x, n);
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> pad_both = [](std::string x, std::string_view y){
        int n = parse_int(y);
        return std::format("{:^{}}", x, n);
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> truncate = [](std::string x, std::string_view y){
        int n = parse_int(y);
        if(x.size() > n) {
            return x.substr(0, n);
        }
        return x;
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> truncate_left = [](std::string x, std::string_view y){
        int n = parse_int(y);
        if(x.size() > n) {
            return x.substr(x.size() - n);
        }
        return x;
    };
    std::add_pointer_t<std::string(std::string, std::string_view)> repeat = [](std::string x, std::string_view y){
        int n = parse_int(y);
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
    std::add_pointer_t<std::string(sys_seconds_double)> strftime = [](sys_seconds_double x){
        return std::format("{:%d/%b/%Y:%H:%M:%S %z}", std::chrono::time_point_cast<std::chrono::seconds>(x));
    };
};

}
