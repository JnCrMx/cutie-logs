module;
#include <string_view>

export module common;

export import :structs;
export import :stencil;
export import :mmdb;

export namespace common {
    constexpr std::string_view project_name = "cutie-logs";
    constexpr std::string_view project_version = "0.1";
}
