module;
#include <string>
#include <unordered_map>
#include <variant>

export module common:structs;

export namespace common {
    struct log_resource {

    };

    using value = std::variant<std::string, int64_t>;

    struct log_entry {
        log_resource resource;
        double timestamp;
        std::string scope;
        std::unordered_map<std::string, value> attributes;
        std::string body;
    };
}
