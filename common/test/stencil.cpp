#include <expected>
#include <iostream>
#include <string>

import common;
import glaze;

struct functions {
    std::add_pointer_t<std::string(int)> to_string = [](int x){
        return std::to_string(x);
    };
    std::add_pointer_t<int(std::string)> from_string = [](std::string x){
        return std::stoi(x);
    };
    std::add_pointer_t<int(int)> add_5 = [](int x){
        return x + 5;
    };
    std::add_pointer_t<int(int, std::string_view)> add = [](int x, std::string_view y){
        return x + std::stoi(std::string{y});
    };
    std::add_pointer_t<std::string(std::string)> test2 = [](std::string x){
        return "Test2: " + x;
    };
};

int main() {
    int a = 5;
    std::cout << *common::eval_expression(a, "add_5 | to_string | from_string | add(13) | to_string | test2", functions{})
        .or_else([](auto&& err) -> std::expected<std::string, std::string> {
            return std::string{"error: "} + err;
        }) << std::endl;

    common::log_entry log{};
    log.timestamp = 1742524030.321027;
    std::cout << *common::stencil("{timestamp} = {timestamp | unix_time | strftime}", log).or_else([](auto&& err) -> std::expected<std::string, std::string> {
        return std::string{"error: "} + err;
    }) << std::endl;

    return 0;
}
