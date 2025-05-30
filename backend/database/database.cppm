module;
#include <chrono>
#include <condition_variable>
#include <deque>
#include <format>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <ranges>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <glaze/util/fast_float.hpp>

export module backend.database;

import pqxx;
import spdlog;
import glaze;

import common;

namespace pqxx {
    export template<> std::string const type_name<common::log_severity>{"log_severity"};
    export template<> struct nullness<common::log_severity> : pqxx::no_null<common::log_severity> {};
    export template<> struct string_traits<common::log_severity> {
        static constexpr bool converts_to_string{true};
        static constexpr bool converts_from_string{true};

        static constexpr zview to_buf(char *begin, char *end, common::log_severity const &value) {
            if(std::to_underlying(value) >= common::log_severity_names.size()) {
                throw pqxx::conversion_error{std::format("Could not convert {} to log_severity", std::to_underlying(value))};
            }
            return common::log_severity_names[static_cast<std::underlying_type_t<common::log_severity>>(value)];
        }
        static char *into_buf(char *begin, char *end, common::log_severity const &value) {
            if(std::to_underlying(value) >= common::log_severity_names.size()) {
                throw pqxx::conversion_error{std::format("Could not convert {} to log_severity", std::to_underlying(value))};
            }
            const char* str = common::log_severity_names[static_cast<std::underlying_type_t<common::log_severity>>(value)];
            auto size = std::char_traits<char>::length(str)+1;
            if(static_cast<std::size_t>(end - begin) < size) {
                throw pqxx::conversion_error{std::format("Buffer too small for log_severity")};
            }
            return std::copy(str, str + size, begin);
        }
        constexpr static std::size_t size_buffer(common::log_severity const &value) noexcept {
            return sizeof("UNSPECIFIED");
        }
        static common::log_severity from_string(std::string_view text) {
            for(std::underlying_type_t<common::log_severity> i{}; i < common::log_severity_names.size(); i++) {
                if(text == common::log_severity_names[i]) {
                    return static_cast<common::log_severity>(i);
                }
            }
            throw pqxx::conversion_error{std::format("Could not convert {} to log_severity", text)};
        }
    };

    export template<> std::string const type_name<common::filter_type>{"filter_type"};
    export template<> struct nullness<common::filter_type> : pqxx::no_null<common::filter_type> {};
    export template<> struct string_traits<common::filter_type> {
        static constexpr bool converts_to_string{true};
        static constexpr bool converts_from_string{true};

        static constexpr zview to_buf(char *begin, char *end, common::filter_type const &value) {
            if(std::to_underlying(value) >= common::filter_type_names.size()) {
                throw pqxx::conversion_error{std::format("Could not convert {} to filter_type", std::to_underlying(value))};
            }
            return common::filter_type_names[static_cast<std::underlying_type_t<common::filter_type>>(value)];
        }
        static char *into_buf(char *begin, char *end, common::filter_type const &value) {
            if(std::to_underlying(value) >= common::filter_type_names.size()) {
                throw pqxx::conversion_error{std::format("Could not convert {} to filter_type", std::to_underlying(value))};
            }
            const char* str = common::filter_type_names[static_cast<std::underlying_type_t<common::filter_type>>(value)];
            auto size = std::char_traits<char>::length(str)+1;
            if(static_cast<std::size_t>(end - begin) < size) {
                throw pqxx::conversion_error{std::format("Buffer too small for filter_type")};
            }
            return std::copy(str, str + size, begin);
        }
        constexpr static std::size_t size_buffer(common::filter_type const &value) noexcept {
            return sizeof("include");
        }
        static common::filter_type from_string(std::string_view text) {
            for(std::underlying_type_t<common::filter_type> i{}; i < common::filter_type_names.size(); i++) {
                if(text == common::filter_type_names[i]) {
                    return static_cast<common::filter_type>(i);
                }
            }
            throw pqxx::conversion_error{std::format("Could not convert {} to filter_type", text)};
        }
    };

    export template<> std::string const type_name<glz::json_t>{"glz::json_t"};
    export template<> struct nullness<glz::json_t> : pqxx::no_null<glz::json_t> {};
    export template<> struct string_traits<glz::json_t> {
        static constexpr bool converts_from_string{true};

        static glz::json_t from_string(std::string_view text) {
            auto ret = glz::read_json<glz::json_t>(text);
            if(!ret) {
                throw pqxx::conversion_error{std::format("Could not convert {} to glz::json_t: {}", text, glz::format_error(ret.error()))};
            }
            return *ret;
        }
    };
}

namespace backend::database {

export template<typename T>
T parse_array(std::string_view sv, const pqxx::connection& conn) {
    pqxx::array<std::ranges::range_value_t<T>> arr{sv, conn};
    return T{arr.cbegin(), arr.cend()};
}

common::standard_filters parse_filters(const pqxx::row& row, const pqxx::connection& conn) {
    common::standard_filters filters;
    filters.resources.values = parse_array<std::set<unsigned int>>(row["filter_resources"].as<std::string>(), conn);
    filters.resources.type = row["filter_resources_type"].as<common::filter_type>();
    filters.scopes.values = parse_array<std::set<std::string>>(row["filter_scopes"].as<std::string>(), conn);
    filters.scopes.type = row["filter_scopes_type"].as<common::filter_type>();
    filters.severities.values = parse_array<std::set<common::log_severity>>(row["filter_severities"].as<std::string>(), conn);
    filters.severities.type = row["filter_severities_type"].as<common::filter_type>();
    filters.attributes.values = parse_array<std::set<std::string>>(row["filter_attributes"].as<std::string>(), conn);
    filters.attributes.type = row["filter_attributes_type"].as<common::filter_type>();
    filters.attribute_values.values = row["filter_attribute_values"].as<glz::json_t>();
    filters.attribute_values.type = row["filter_attribute_values_type"].as<common::filter_type>();
    return filters;
}

export class Database {
    public:
        constexpr static unsigned int default_worker_count = 4;

        Database(const std::string& connection_string, unsigned int worker_count = default_worker_count)
            : logger(spdlog::default_logger()->clone("database"))
        {
            connections.reserve(worker_count);
            for(int i = 0; i < worker_count; i++) {
                connections.emplace_back(connection_string);
                auto& conn = connections.back();
                conn.set_session_var("application_name", std::format("backend-worker-{}", i));
            }
        }

        void run_migrations();
        void ensure_consistency();
        void ensure_consistency(pqxx::connection& conn);
        void ensure_consistency(pqxx::transaction_base& txn);

        void start_workers() {
            for(int i = 0; i < connections.size(); i++) {
                auto& thread = threads.emplace_back([this, i](std::stop_token st) {
                    worker(i, connections[i], st);
                });
                pthread_setname_np(thread.native_handle(), std::format("backend-worker-{}", i).c_str());
            }
        }

        std::future<void> queue_work(std::move_only_function<void(pqxx::connection&)>&& work) {
            std::unique_lock lock(mutex);
            queue.emplace_back(std::move(work), std::promise<void>{});
            cv.notify_one();

            return queue.back().second.get_future();
        }
        unsigned int ensure_resource(pqxx::connection& conn, const glz::json_t& attributes, unsigned int tries = 3) {
            try {
                pqxx::work txn(conn);
                pqxx::result res = txn.exec(pqxx::prepped{"ensure_resource"}, pqxx::params{attributes.dump().value()});
                txn.commit();
                return res[0][0].as<unsigned int>();
            } catch (const pqxx::deadlock_detected& e) {
                if(tries > 0) {
                    logger->warn("Deadlock detected in ensure_resource, retrying");
                    return ensure_resource(conn, attributes, tries - 1);
                }
                logger->error("Deadlock detected in ensure_resource, giving up");
                throw;
            }
        };
        void insert_log(pqxx::connection& conn, unsigned int resource, std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> timestamp,
            const std::string& scope, common::log_severity severity, const glz::json_t& attributes, const glz::json_t& body, unsigned int tries = 3)
        {
            try {
                pqxx::work txn(conn);
                std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double, std::chrono::seconds::period>> ts_seconds = timestamp;
                txn.exec(pqxx::prepped{"insert_log"}, {resource, ts_seconds.time_since_epoch().count(), scope, severity, attributes.dump().value(), body.dump().value()});

                if(attributes.is_object() && attributes.size() > 0){
                    std::string select_for_update = "SELECT * FROM log_attributes WHERE attribute IN (";
                    bool first = true;
                    for(const auto& [key, value] : attributes.get_object()) {
                        if(!first) {
                            select_for_update += ", ";
                        }
                        select_for_update += txn.quote(key);
                        first = false;
                    }
                    select_for_update += ") FOR UPDATE";
                    txn.exec(select_for_update);
                }

                if(attributes.is_object()) {
                    auto view = attributes.get_object() | std::views::keys;
                    std::set<std::string> keys(view.begin(), view.end());

                    for(const auto& key : keys) {
                        const auto& value = attributes[key];
                        txn.exec(pqxx::prepped{"update_attribute"}, {key, 1,
                            static_cast<int>(value.is_null()), static_cast<int>(value.is_number()), static_cast<int>(value.is_string()),
                            static_cast<int>(value.is_boolean()), static_cast<int>(value.is_array()), static_cast<int>(value.is_object())
                        });
                    }
                }
                txn.commit();
            } catch (const pqxx::deadlock_detected& e) {
                if(tries > 0) {
                    logger->warn("Deadlock detected in insert_log, retrying");
                    insert_log(conn, resource, timestamp, scope, severity, attributes, body, tries - 1);
                    return;
                }
                logger->error("Deadlock detected in insert_log, giving up");
                throw;
            } catch (const pqxx::unique_violation& c) {
                if(tries > 0) {
                    logger->warn("Unique violation detected in insert_log, retrying with timestamp + 1 us");
                    insert_log(conn, resource, timestamp + std::chrono::microseconds(1), scope, severity, attributes, body, tries - 1);
                    return;
                }
                logger->error("Unique violation detected in insert_log, giving up");
            }
        }

        std::map<unsigned int, common::cleanup_rule> get_cleanup_rules(pqxx::transaction_base& txn) {
            auto result = txn.exec(pqxx::prepped{"get_cleanup_rules"});
            std::map<unsigned int, common::cleanup_rule> rules;

            for(const auto& row : result) {
                unsigned int id = row["id"].as<unsigned int>();
                common::cleanup_rule& rule = rules[id];

                rule.id = id;
                rule.name = row["name"].as<std::string>();
                rule.description = row["description"].as<std::string>();
                rule.enabled = row["enabled"].as<bool>();
                rule.execution_interval = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["execution_interval_s"].as<double>()});

                rule.filter_minimum_age = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["filter_minimum_age_s"].as<double>()});
                rule.filters = parse_filters(row, txn.conn());

                rule.created_at = std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["created_at_s"].as<double>()})};
                rule.updated_at = std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["updated_at_s"].as<double>()})};
                rule.last_execution = row["last_execution_s"].as<std::optional<double>>().transform([](const auto& d) {
                    return std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::duration<double>{d})};
                });
            }
            return rules;
        }
        std::map<unsigned int, common::alert_rule> get_alert_rules(pqxx::transaction_base& txn) {
            auto result = txn.exec(pqxx::prepped{"get_alert_rules"});
            std::map<unsigned int, common::alert_rule> rules;

            for(const auto& row : result) {
                unsigned int id = row["id"].as<unsigned int>();
                common::alert_rule& rule = rules[id];

                rule.id = id;
                rule.name = row["name"].as<std::string>();
                rule.description = row["description"].as<std::string>();
                rule.enabled = row["enabled"].as<bool>();

                rule.filters = parse_filters(row, txn.conn());

                rule.notification_provider = row["notification_provider"].as<std::string>();
                rule.notification_options = row["notification_options"].as<glz::json_t>();

                rule.created_at = std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["created_at_s"].as<double>()})};
                rule.updated_at = std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["updated_at_s"].as<double>()})};
                rule.last_alert = row["last_alert_s"].as<std::optional<double>>().transform([](const auto& d) {
                    return std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::duration<double>{d})};
                });
                rule.last_alert_successful = row["last_alert_successful"].as<std::optional<bool>>();
                rule.last_alert_message = row["last_alert_message"].as<std::optional<std::string>>();
            }
            return rules;
        }

    private:
        void prepare_statements(pqxx::connection& conn) {
            conn.prepare("insert_log",
                "INSERT INTO logs (resource, timestamp, scope, severity, attributes, body) "
                "VALUES ($1, to_timestamp($2::double precision), $3, $4, $5::jsonb, $6::jsonb)");
            conn.prepare("update_attribute",
                "INSERT INTO log_attributes (attribute, count, count_null, count_number, count_string, count_boolean, count_array, count_object) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
                "ON CONFLICT (attribute) DO UPDATE SET "
                "count = log_attributes.count + EXCLUDED.count, "
                "count_null = log_attributes.count_null + EXCLUDED.count_null, "
                "count_number = log_attributes.count_number + EXCLUDED.count_number, "
                "count_string = log_attributes.count_string + EXCLUDED.count_string, "
                "count_boolean = log_attributes.count_boolean + EXCLUDED.count_boolean, "
                "count_array = log_attributes.count_array + EXCLUDED.count_array, "
                "count_object = log_attributes.count_object + EXCLUDED.count_object");
            conn.prepare("ensure_resource",
                "INSERT INTO log_resources (attributes) "
                "VALUES ($1::jsonb) "
                "ON CONFLICT (attributes) DO UPDATE SET "
                "attributes = EXCLUDED.attributes "
                "RETURNING id");
            conn.prepare("get_count",
                "SELECT COUNT(*) FROM logs");
            conn.prepare("get_resources",
                "SELECT res.id AS id, extract(epoch from res.created_at) AS created_at, res.attributes AS attributes, COUNT(*) AS count "
                "FROM log_resources res, logs WHERE id = resource GROUP BY id");
            conn.prepare("get_attributes",
                "SELECT attribute, count FROM log_attributes");
            conn.prepare("get_scopes",
                "SELECT scope, COUNT(*) as count FROM logs GROUP BY scope");
            conn.prepare("get_cleanup_rules",
                "SELECT id, name, description, enabled, extract(epoch from execution_interval) AS execution_interval_s, "
                "extract(epoch from filter_minimum_age) AS filter_minimum_age_s, "
                "filter_resources, filter_resources_type, "
                "filter_scopes, filter_scopes_type, "
                "filter_severities, filter_severities_type, "
                "filter_attributes, filter_attributes_type, "
                "filter_attribute_values, filter_attribute_values_type, "
                "extract(epoch from created_at) AS created_at_s, extract(epoch from updated_at) AS updated_at_s, "
                "extract(epoch from last_execution) AS last_execution_s "
                "FROM cleanup_rules");
            conn.prepare("insert_cleanup_rule",
                "INSERT INTO cleanup_rules (name, description, enabled, execution_interval, filter_minimum_age, "
                "filter_resources, filter_resources_type, filter_scopes, filter_scopes_type, "
                "filter_severities, filter_severities_type, filter_attributes, filter_attributes_type, "
                "filter_attribute_values, filter_attribute_values_type) "
                "VALUES ($1, $2, $3, $4*'1 second'::interval, $5*'1 second'::interval, "
                "$6, $7, $8, $9, $10, $11, $12, $13, $14::jsonb, $15) "
                "RETURNING id, extract(epoch from created_at) AS created_at_s, extract(epoch from updated_at) AS updated_at_s");
            conn.prepare("update_cleanup_rule",
                "UPDATE cleanup_rules SET name = $1, description = $2, enabled = $3, "
                "execution_interval = $4*'1 second'::interval, filter_minimum_age = $5*'1 second'::interval, "
                "filter_resources = $6, filter_resources_type = $7, filter_scopes = $8, filter_scopes_type = $9, "
                "filter_severities = $10, filter_severities_type = $11, filter_attributes = $12, filter_attributes_type = $13, "
                "filter_attribute_values = $14::jsonb, filter_attribute_values_type = $15, updated_at = now() "
                "WHERE id = $16 "
                "RETURNING id, extract(epoch from created_at) AS created_at_s, extract(epoch from updated_at) AS updated_at_s, extract(epoch from last_execution) AS last_execution_s");
            conn.prepare("delete_cleanup_rule",
                "DELETE FROM cleanup_rules WHERE id = $1");
            conn.prepare("get_alert_rules",
                "SELECT id, name, description, enabled, "
                "filter_resources, filter_resources_type, "
                "filter_scopes, filter_scopes_type, "
                "filter_severities, filter_severities_type, "
                "filter_attributes, filter_attributes_type, "
                "filter_attribute_values, filter_attribute_values_type, "
                "notification_provider, notification_options, "
                "extract(epoch from created_at) AS created_at_s, extract(epoch from updated_at) AS updated_at_s, "
                "extract(epoch from last_alert) AS last_alert_s, last_alert_successful, last_alert_message "
                "FROM alert_rules");
            conn.prepare("update_alert_result",
                "UPDATE alert_rules SET last_alert = now(), last_alert_successful = $2, last_alert_message = $3 "
                "WHERE id = $1");
            conn.prepare("insert_alert_rule",
                "INSERT INTO alert_rules (name, description, enabled, notification_provider, notification_options, "
                "filter_resources, filter_resources_type, filter_scopes, filter_scopes_type, "
                "filter_severities, filter_severities_type, filter_attributes, filter_attributes_type, "
                "filter_attribute_values, filter_attribute_values_type) "
                "VALUES ($1, $2, $3, $4, $5::jsonb, "
                "$6, $7, $8, $9, $10, $11, $12, $13, $14::jsonb, $15) "
                "RETURNING id, extract(epoch from created_at) AS created_at_s, extract(epoch from updated_at) AS updated_at_s");
            conn.prepare("update_alert_rule",
                "UPDATE alert_rules SET name = $1, description = $2, enabled = $3, "
                "notification_provider = $4, notification_options = $5::jsonb, "
                "filter_resources = $6, filter_resources_type = $7, filter_scopes = $8, filter_scopes_type = $9, "
                "filter_severities = $10, filter_severities_type = $11, filter_attributes = $12, filter_attributes_type = $13, "
                "filter_attribute_values = $14::jsonb, filter_attribute_values_type = $15, updated_at = now() "
                "WHERE id = $16 "
                "RETURNING id, extract(epoch from created_at) AS created_at_s, extract(epoch from updated_at) AS updated_at_s, "
                "extract(epoch from last_alert) AS last_alert_s, last_alert_successful, last_alert_message");
            conn.prepare("delete_alert_rule",
                "DELETE FROM alert_rules WHERE id = $1");
        }

        void worker(unsigned int id, pqxx::connection& conn, std::stop_token st) {
            logger->debug("Worker {} started", id);
            prepare_statements(conn);

            while(!st.stop_requested()) {
                // first of all, make sure we receive notifications
                conn.get_notifs();

                std::move_only_function<void(pqxx::connection&)> work;
                std::promise<void> promise;
                {
                    std::unique_lock lock(mutex);
                    if(queue.empty()) {
                        // timeout, so we will check for notifications, even if we don't have work
                        bool no_timeout = cv.wait_for(lock, std::chrono::seconds(10), [this, &st] {
                            return st.stop_requested() || !queue.empty();
                        });
                        if(!no_timeout) {
                            continue;
                        }
                    }

                    if(st.stop_requested()) {
                        break;
                    }

                    if(queue.empty()) {
                        continue;
                    }

                    work = std::move(queue.front().first);
                    promise = std::move(queue.front().second);
                    queue.pop_front();
                }

                if(st.stop_requested()) {
                    break;
                }

                work(conn);
                promise.set_value();
            }
        }

        std::shared_ptr<spdlog::logger> logger;
        std::vector<pqxx::connection> connections;
        std::vector<std::jthread> threads;
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::pair<std::move_only_function<void(pqxx::connection&)>, std::promise<void>>> queue;
};

}
