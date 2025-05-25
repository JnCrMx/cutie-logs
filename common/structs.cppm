module;
#include <array>
#include <chrono>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module common:structs;

import glaze;

export namespace common {
    template<typename T>
    concept serializable_beve = glz::read_supported<T, glz::BEVE> && glz::write_supported<T, glz::BEVE>;

    template<typename T>
    concept serializable_json = glz::read_supported<T, glz::JSON> && glz::write_supported<T, glz::JSON>;

    template<typename T>
    concept serializable = serializable_beve<T> && serializable_json<T>;

    struct shared_settings {
        struct {
            std::optional<std::string> country_url;
            std::optional<std::string> asn_url;
            std::optional<std::string> city_url;
        } geoip;
    };
    static_assert(serializable<shared_settings>);

    struct log_resource {
        glz::json_t attributes;
        double created_at;

        std::optional<std::string> guess_name() const {
            if(attributes.contains("service.name")) {
                return attributes.at("service.name").get_string();
            }
            if(attributes.contains("k8s.namespace.name")) {
                if(attributes.contains("k8s.pod.name")) {
                    return std::format("{}/{}", attributes.at("k8s.namespace.name").get_string(), attributes.at("k8s.pod.name").get_string());
                }
            }
            return std::nullopt;
        }
    };
    static_assert(serializable<log_resource>);

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
    static_assert(serializable<log_entry>);

    struct log_entry_stencil_object {
        const log_resource* resource;
        const log_entry* log;

        static constexpr auto root = "log";
    };

    struct logs_response {
        std::vector<common::log_entry> logs;
    };
    static_assert(serializable<logs_response>);

    struct logs_attributes_response {
        unsigned int total_logs;
        std::unordered_map<std::string, unsigned int> attributes;
    };
    static_assert(serializable<logs_attributes_response>);

    struct logs_scopes_response {
        unsigned int total_logs;
        std::unordered_map<std::string, unsigned int> scopes;
    };
    static_assert(serializable<logs_scopes_response>);

    struct logs_resources_response {
        std::unordered_map<unsigned int, std::tuple<log_resource, unsigned int>> resources;
    };
    static_assert(serializable<logs_resources_response>);

    enum class filter_type {
        INCLUDE, EXCLUDE
    };
    constexpr std::array filter_type_names = {
        "INCLUDE", "EXCLUDE"
    };

    template<typename T>
    struct filter {
        filter_type type = filter_type::EXCLUDE;
        T values;
    };

    struct standard_filters {
        filter<std::set<unsigned int>> resources;
        filter<std::set<std::string>> scopes;
        filter<std::set<log_severity>> severities;
        filter<std::set<std::string>> attributes;
        filter<glz::json_t> attribute_values = {{}, glz::json_t::object_t{}};

        bool match(const log_entry& entry) const {
            if(resources.type == filter_type::INCLUDE && !resources.values.contains(entry.resource)) { return false; }
            if(resources.type == filter_type::EXCLUDE &&  resources.values.contains(entry.resource)) { return false; }
            if(scopes.type == filter_type::INCLUDE && !scopes.values.contains(entry.scope)) { return false; }
            if(scopes.type == filter_type::EXCLUDE &&  scopes.values.contains(entry.scope)) { return false; }
            if(severities.type == filter_type::INCLUDE && !severities.values.contains(entry.severity)) { return false; }
            if(severities.type == filter_type::EXCLUDE &&  severities.values.contains(entry.severity)) { return false; }
            if(attributes.type == filter_type::INCLUDE) {
                for(const auto& a : attributes.values) {
                    if(!entry.attributes.contains(a)) { return false; }
                }
            } else {
                for(const auto& a : attributes.values) {
                    if(entry.attributes.contains(a)) { return false; }
                }
            }
            // TODO: attribute_values filtering
            return true;
        }
    };
    static_assert(serializable<standard_filters>);

    struct cleanup_rule {
        unsigned int id;
        std::string name;
        std::string description;
        bool enabled = false;
        std::chrono::seconds execution_interval;

        std::chrono::seconds filter_minimum_age;
        standard_filters filters;

        std::chrono::sys_seconds created_at;
        std::chrono::sys_seconds updated_at;
        std::optional<std::chrono::sys_seconds> last_execution;
    };
    static_assert(serializable<cleanup_rule>);

    struct cleanup_rules_response {
        std::map<unsigned int, cleanup_rule> rules;
    };
    static_assert(serializable<cleanup_rules_response>);

    struct alert_rule {
        unsigned int id;
        std::string name;
        std::string description;
        bool enabled = false;

        standard_filters filters;

        std::string notification_provider;
        glz::json_t notification_options;

        std::chrono::sys_seconds created_at;
        std::chrono::sys_seconds updated_at;
        std::optional<std::chrono::sys_seconds> last_alert;
        std::optional<bool> last_alert_successful;
        std::optional<std::string> last_alert_message;

        bool match(const log_entry& entry) const {
            if(!enabled) return false;
            if(!filters.match(entry)) return false;
            return true;
        }
    };
    static_assert(serializable<alert_rule>);

    struct alert_stencil_object {
        const alert_rule* rule;
        const log_resource* resource;
        const log_entry* log;

        static constexpr auto root = "log";
    };
}

export namespace glz {
    template<uint32_t format, typename Rep, typename Period>
    struct from<format, std::chrono::duration<Rep, Period>> {
        template<auto Opts>
        static void op(std::chrono::duration<Rep, Period>& value, auto&&... args) {
            Rep raw;
            parse<format>::template op<Opts>(raw, args...);
            value = std::chrono::duration<Rep, Period>(raw);
        }
    };

    template<uint32_t format, typename Rep, typename Period>
    struct to<format, std::chrono::duration<Rep, Period>> {
        template<auto Opts>
        static void op(const std::chrono::duration<Rep, Period>& value, auto&&... args) {
            serialize<format>::template op<Opts>(value.count(), args...);
        }
    };

    template<uint32_t format, typename Clock, typename Duration>
    struct from<format, std::chrono::time_point<Clock, Duration>> {
        template<auto Opts>
        static void op(std::chrono::time_point<Clock, Duration>& value, auto&&... args) {
            Duration raw;
            parse<format>::template op<Opts>(raw, args...);
            value = std::chrono::time_point<Clock, Duration>(raw);
        }
    };

    template<uint32_t format, typename Clock, typename Duration>
    struct to<format, std::chrono::time_point<Clock, Duration>> {
        template<auto Opts>
        static void op(const std::chrono::time_point<Clock, Duration>& value, auto&&... args) {
            serialize<format>::template op<Opts>(value.time_since_epoch(), args...);
        }
    };
}

export template<>
struct std::formatter<common::log_severity> {
    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<class FmtContext>
    constexpr FmtContext::iterator format(common::log_severity severity, FmtContext& ctx) const {
        return std::format_to(ctx.out(), "{}", common::log_severity_names[std::to_underlying(severity)]);
    }
};

export template<>
struct std::formatter<common::filter_type> {
    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<class FmtContext>
    constexpr FmtContext::iterator format(common::filter_type type, FmtContext& ctx) const {
        return std::format_to(ctx.out(), "{}", common::filter_type_names[std::to_underlying(type)]);
    }
};
