module;
#include <string_view>

export module common;

export import :structs;
export import :stencil;
export import :stencil_functions;
export import :mmdb;
export import :utils;

export namespace common {
    constexpr std::string_view project_name = "cutie-logs";
    constexpr std::string_view project_version = "0.1";

    constexpr std::string_view default_stencil = "[{timestamp | from_timestamp | iso_date_time}] [{resource}-{scope}] [{severity}] {body}";
}
