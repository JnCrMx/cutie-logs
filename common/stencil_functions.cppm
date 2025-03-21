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
        return x.get_number();
    };
    std::add_pointer_t<std::string(glz::json_t)> get_string = [](glz::json_t x){
        return x.get_string();
    };
    std::add_pointer_t<bool(glz::json_t)> get_boolean = [](glz::json_t x){
        return x.get_boolean();
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
