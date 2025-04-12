module;
#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

export module common:structs;

import glaze;

export namespace common {
    struct shared_settings {
        struct {
            std::optional<std::string> country_url;
            std::optional<std::string> asn_url;
            std::optional<std::string> city_url;
        } geoip;
    };

    struct log_resource {
        glz::json_t attributes;
        double created_at;
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

    struct log_entry {
        unsigned int resource;
        double timestamp;
        std::string scope;
        common::log_severity severity;
        glz::json_t attributes;
        glz::json_t body;
    };

    struct log_entry_with_resource {
        log_resource* resource;
        log_entry* log;

        static constexpr auto root = "log";
    };

    struct logs_response {
        std::vector<common::log_entry> logs;
    };
    struct logs_attributes_response {
        unsigned int total_logs;
        std::unordered_map<std::string, unsigned int> attributes;
    };
    struct logs_scopes_response {
        unsigned int total_logs;
        std::unordered_map<std::string, unsigned int> scopes;
    };
    struct logs_resources_response {
        std::unordered_map<unsigned int, std::tuple<log_resource, unsigned int>> resources;
    };
}
