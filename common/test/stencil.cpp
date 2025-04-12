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

    common::stencil_functions base{};
};

int main() {
    int a = 5;
    functions fn{};
    std::cout << *common::eval_expression(a, "add_5 | to_string | from_string | add(13) | to_string | test2", fn, fn)
        .or_else([](auto&& err) -> std::expected<std::string, std::string> {
            return std::string{"error: "} + err;
        }) << std::endl;

    common::log_entry log{};
    log.timestamp = 1742524030.321027;
    std::cout << *common::stencil("{timestamp} = {timestamp | from_timestamp | strftime}", log).or_else([](auto&& err) -> std::expected<std::string, std::string> {
        return std::string{"error: "} + err;
    }) << std::endl;

    common::log_resource resource{};
    resource.created_at = 1732524030.321027;
    resource.attributes = glz::json_t::object_t{{"key", "value"}};
    std::cout << *common::stencil("{created_at} = {created_at | from_timestamp | strftime} | {attributes.key}", resource).or_else([](auto&& err) -> std::expected<std::string, std::string> {
        return std::string{"error: "} + err;
    }) << std::endl;

    common::log_entry_with_resource log_with_resource{
        .resource = &resource,
        .log = &log
    };
    std::cout << *common::stencil("resource: {resource.created_at} = {resource.created_at | from_timestamp | strftime} | {resource.attributes.key} | {.timestamp} = {.timestamp | from_timestamp | strftime}", log_with_resource, functions{}).or_else([](auto&& err) -> std::expected<std::string, std::string> {
        return std::string{"error: "} + err;
    }) << std::endl;

    return 0;
}
