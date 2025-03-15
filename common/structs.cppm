module;
#include <array>
#include <string>
#include <unordered_map>
#include <variant>

export module common:structs;

export namespace common {
    struct log_resource {

    };

    enum class log_severity {
        UNSPECIFIED = 0,
        TRACE, TRACE2, TRACE3, TRACE4,
        DEBUG, DEBUG2, DEBUG3, DEBUG4,
        INFO, INFO2, INFO3, INFO4,
        WARN, WARN2, WARN3, WARN4,
        ERROR, ERROR2, ERROR3, ERROR4,
        FATAL, FATAL2, FATAL3, FATAL4
    };

    constexpr std::array log_severity_names = {
        "UNSPECIFIED",
        "TRACE", "TRACE2", "TRACE3", "TRACE4",
        "DEBUG", "DEBUG2", "DEBUG3", "DEBUG4",
        "INFO", "INFO2", "INFO3", "INFO4",
        "WARN", "WARN2", "WARN3", "WARN4",
        "ERROR", "ERROR2", "ERROR3", "ERROR4",
        "FATAL", "FATAL2", "FATAL3", "FATAL4"
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
