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
        [[nodiscard]] static constexpr std::string_view to_buf(std::span<char> buf, common::log_severity const &value, ctx c = {}) {
            if(std::to_underlying(value) >= common::log_severity_names.size()) {
                throw pqxx::conversion_error{std::format("Could not convert {} to log_severity", std::to_underlying(value))};
            }
            return common::log_severity_names[static_cast<std::underlying_type_t<common::log_severity>>(value)];
        }
        [[nodiscard]] static constexpr std::size_t size_buffer(common::log_severity const &value) noexcept {
            constexpr std::size_t size = [](){
                std::size_t size = 0;
                for(const auto& name : common::log_severity_names) {
                    size = std::max(size, std::string_view::traits_type::length(name));
                }
                return size;
            }();
            return size;
        }

        [[nodiscard]] static constexpr common::log_severity from_string(std::string_view text, ctx c = {}) {
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
        [[nodiscard]] static constexpr std::string_view to_buf(std::span<char> buf, common::filter_type const &value, ctx c = {}) {
            if(std::to_underlying(value) >= common::filter_type_names.size()) {
                throw pqxx::conversion_error{std::format("Could not convert {} to filter_type", std::to_underlying(value))};
            }
            return common::filter_type_names[static_cast<std::underlying_type_t<common::filter_type>>(value)];
        }
        [[nodiscard]] static constexpr std::size_t size_buffer(common::filter_type const &value) noexcept {
            constexpr std::size_t size = [](){
                std::size_t size = 0;
                for(const auto& name : common::filter_type_names) {
                    size = std::max(size, std::string_view::traits_type::length(name));
                }
                return size;
            }();
            return size;
        }
        [[nodiscard]] static constexpr common::filter_type from_string(std::string_view text, ctx c = {}) {
            for(std::underlying_type_t<common::filter_type> i{}; i < common::filter_type_names.size(); i++) {
                if(text == common::filter_type_names[i]) {
                    return static_cast<common::filter_type>(i);
                }
            }
            throw pqxx::conversion_error{std::format("Could not convert {} to filter_type", text)};
        }
    };

    export template<> std::string const type_name<common::rule_action>{"rule_action"};
    export template<> struct nullness<common::rule_action> : pqxx::no_null<common::rule_action> {};
    export template<> struct string_traits<common::rule_action> {
        [[nodiscard]] static constexpr std::string_view to_buf(std::span<char> buf, common::rule_action const &value, ctx c = {}) {
            if(std::to_underlying(value) >= common::rule_action_names.size()) {
                throw pqxx::conversion_error{std::format("Could not convert {} to rule_action", std::to_underlying(value))};
            }
            return common::rule_action_names[static_cast<std::underlying_type_t<common::rule_action>>(value)];
        }
        [[nodiscard]] static constexpr std::size_t size_buffer(common::rule_action const &value) noexcept {
            constexpr std::size_t size = [](){
                std::size_t size = 0;
                for(const auto& name : common::rule_action_names) {
                    size = std::max(size, std::string_view::traits_type::length(name));
                }
                return size;
            }();
            return size;
        }
        [[nodiscard]] static constexpr common::rule_action from_string(std::string_view text, ctx c = {}) {
            for(std::underlying_type_t<common::rule_action> i{}; i < common::rule_action_names.size(); i++) {
                if(text == common::rule_action_names[i]) {
                    return static_cast<common::rule_action>(i);
                }
            }
            throw pqxx::conversion_error{std::format("Could not convert {} to rule_action", text)};
        }
    };

    export template<> std::string const type_name<glz::generic>{"glz::generic"};
    export template<> struct nullness<glz::generic> : pqxx::no_null<glz::generic> {};
    export template<> struct string_traits<glz::generic> {
        [[nodiscard]] static constexpr glz::generic from_string(std::string_view text, ctx c = {}) {
            auto ret = glz::read_json<glz::generic>(text);
            if(!ret) {
                throw pqxx::conversion_error{std::format("Could not convert {} to glz::generic: {}", text, glz::format_error(ret.error()))};
            }
            return *ret;
        }

        [[nodiscard]] static constexpr std::size_t size_buffer_(glz::generic const &value) noexcept {
            if(value.is_null()) return 4;
            if(value.is_boolean()) return 5;
            if(value.is_number()) return 32;
            if(value.is_string()) {
                return value.get_string().size() * 2 + 2;
            }
            if(value.is_array()) {
                const auto& array = value.get_array();
                if(array.empty()) return 2;

                std::size_t total = 2;
                for(const auto& item : array) {
                    total += size_buffer_(item) + 1;
                }
                return total;
            }
            if(value.is_object()) {
                const auto& object = value.get_object();
                if(object.empty()) return 2;

                std::size_t total = 2;
                for(const auto& [key, value] : object) {
                    total += key.size() * 2 + 2 + 1 + size_buffer_(value) + 1;
                }
                return total;
            }
            return 0;
        }

        [[nodiscard]] static constexpr std::size_t size_buffer(glz::generic const &value) noexcept {
            return size_buffer_(value) + glz::write_padding_bytes;
        }

        [[nodiscard]] static std::string_view to_buf(std::span<char> buf, glz::generic const &value, ctx c = {}) {
            auto ec = glz::write_json(value, buf);
            if(ec) [[unlikely]] {
                if(ec.ec == glz::error_code::buffer_overflow) {
                    std::string json = glz::write_json(value).value(); // allocating here is okay-ish, because we are in the exceptional case
                    throw pqxx::conversion_overrun{std::format("buffer overflow at count = {} for buffer size = {} and json = \"{}\" with actual size = {}",
                        ec.count, buf.size(), json, json.size())};
                } else {
                    throw pqxx::conversion_error{glz::format_error(ec)};
                }
            }
            return std::string_view{buf.data(), ec.count};
        }
    };
}

namespace backend::database {

common::standard_filters parse_filters(const pqxx::row_ref& row, const pqxx::connection& conn) {
    common::standard_filters filters;
    filters.resources.values = row["filter_resources"].as<std::set<unsigned int>>();
    filters.resources.type = row["filter_resources_type"].as<common::filter_type>();
    filters.scopes.values = row["filter_scopes"].as<std::set<std::string>>();
    filters.scopes.type = row["filter_scopes_type"].as<common::filter_type>();
    filters.severities.values = row["filter_severities"].as<std::set<common::log_severity>>();
    filters.severities.type = row["filter_severities_type"].as<common::filter_type>();
    filters.attributes.values = row["filter_attributes"].as<std::set<std::string>>();
    filters.attributes.type = row["filter_attributes_type"].as<common::filter_type>();
    filters.attribute_values.values = row["filter_attribute_values"].as<glz::generic>();
    filters.attribute_values.type = row["filter_attribute_values_type"].as<common::filter_type>();
    return filters;
}

export class Database {
    public:
        constexpr static unsigned int default_worker_count = 4;

        Database(const std::string& connection_string, unsigned int worker_count = default_worker_count)
            : connection_string(connection_string), logger(spdlog::default_logger()->clone("database"))
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
        unsigned int ensure_resource(pqxx::connection& conn, const glz::generic& attributes, unsigned int tries = 3) {
            try {
                pqxx::work txn(conn);
                pqxx::result res = txn.exec(pqxx::prepped{"find_resource"}, pqxx::params{txn, attributes});
                if(res.size() == 1) {
                    return res[0][0].as<unsigned int>();
                } else if(res.size() == 0) {
                    res = txn.exec(pqxx::prepped{"insert_resource"}, pqxx::params{txn, attributes});
                    txn.commit();
                    return res[0][0].as<unsigned int>();
                } else {
                    logger->critical("Unexpected row count in ensure_resource: expected 0 or 1, but got {}", res.size());
                    throw std::logic_error("Unexpected row count in ensure_resource");
                }
            } catch (const pqxx::deadlock_detected& e) {
                if(tries > 0) {
                    logger->warn("Deadlock detected in ensure_resource, retrying");
                    return ensure_resource(conn, attributes, tries - 1);
                }
                logger->error("Deadlock detected in ensure_resource, giving up");
                throw;
            }
        };
        void create_partition(pqxx::connection& conn, std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> timestamp) {
            std::chrono::sys_days day = std::chrono::floor<std::chrono::days>(timestamp);
            std::chrono::year_month_day ymd{day};
            std::string partition_name = std::format("logs_{:04}{:02}{:02}",
                static_cast<int>(ymd.year()),
                static_cast<unsigned int>(ymd.month()),
                static_cast<unsigned int>(ymd.day()));

            auto from = std::chrono::time_point_cast<std::chrono::seconds>(day);
            auto to = std::chrono::time_point_cast<std::chrono::seconds>(day + std::chrono::days{1});

            std::string create_partition_sql = std::format(
                "CREATE TABLE IF NOT EXISTS {} PARTITION OF logs FOR VALUES FROM (to_timestamp({})) TO (to_timestamp({}))",
                partition_name,
                from.time_since_epoch().count(),
                to.time_since_epoch().count()
            );
            logger->info("Creating partition with SQL: {}", create_partition_sql);

            pqxx::work txn(conn);
            txn.exec(create_partition_sql);
            txn.commit();
        }
        void insert_log(pqxx::connection& conn, unsigned int resource, std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> timestamp,
            const std::string& scope, common::log_severity severity, const glz::generic& attributes, const glz::generic& body, unsigned int tries = 3)
        {
            try {
                pqxx::work txn(conn);
                std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double, std::chrono::seconds::period>> ts_seconds = timestamp;
                txn.exec(pqxx::prepped{"insert_log"}, pqxx::params{txn, resource, ts_seconds.time_since_epoch().count(), scope, severity, attributes, body});

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
                        txn.exec(pqxx::prepped{"update_attribute"}, pqxx::params{key, 1,
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
            } catch (const pqxx::check_violation& c) {
                logger->debug("No partition for timestamp {}, creating partition and retrying", timestamp);
                create_partition(conn, timestamp);
                insert_log(conn, resource, timestamp, scope, severity, attributes, body, tries); // do not decrease tries, because this error is expected
                return;
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

                rule.action = row["action"].as<common::rule_action>();
                rule.action_options = row["action_options"].as<std::optional<glz::generic>>();

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
                rule.notification_options = row["notification_options"].as<glz::generic>();

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
            conn.prepare("find_resource",
                "SELECT id FROM log_resources "
                "WHERE attributes = $1::jsonb");
            conn.prepare("insert_resource",
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
                "action, action_options, "
                "extract(epoch from created_at) AS created_at_s, extract(epoch from updated_at) AS updated_at_s, "
                "extract(epoch from last_execution) AS last_execution_s "
                "FROM cleanup_rules");
            conn.prepare("insert_cleanup_rule",
                "INSERT INTO cleanup_rules (name, description, enabled, execution_interval, filter_minimum_age, "
                "filter_resources, filter_resources_type, filter_scopes, filter_scopes_type, "
                "filter_severities, filter_severities_type, filter_attributes, filter_attributes_type, "
                "filter_attribute_values, filter_attribute_values_type, action, action_options) "
                "VALUES ($1, $2, $3, $4*'1 second'::interval, $5*'1 second'::interval, "
                "$6, $7, $8, $9, $10, $11, $12, $13, $14::jsonb, $15, $16, $17::jsonb) "
                "RETURNING id, extract(epoch from created_at) AS created_at_s, extract(epoch from updated_at) AS updated_at_s");
            conn.prepare("update_cleanup_rule",
                "UPDATE cleanup_rules SET name = $1, description = $2, enabled = $3, "
                "execution_interval = $4*'1 second'::interval, filter_minimum_age = $5*'1 second'::interval, "
                "filter_resources = $6, filter_resources_type = $7, filter_scopes = $8, filter_scopes_type = $9, "
                "filter_severities = $10, filter_severities_type = $11, filter_attributes = $12, filter_attributes_type = $13, "
                "filter_attribute_values = $14::jsonb, filter_attribute_values_type = $15, "
                "action = $16, action_options = $17::jsonb, updated_at = now() "
                "WHERE id = $18 "
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
            conn.prepare("complete_cleanup_rule",
                "UPDATE cleanup_rules SET last_execution = NOW() WHERE id = $1");
            conn.prepare("update_log_attributes",
                "UPDATE logs SET attributes = $4::jsonb WHERE resource = $1 AND timestamp = to_timestamp($2::double precision) AND scope = $3");
            conn.prepare("get_indices",
                "SELECT c.relname AS index_name, parent_c.relname AS parent_index, obj_description(c.oid, 'pg_class') AS index_comment, NOT i.indisvalid AS is_invalid "
                "FROM pg_class c JOIN pg_index i ON c.oid = i.indexrelid LEFT JOIN pg_inherits inh ON c.oid = inh.inhrelid LEFT JOIN pg_class parent_c ON inh.inhparent = parent_c.oid "
                "WHERE c.relkind IN ('i', 'I') AND c.relname LIKE 'logs_managed_%';");
        }

        void reconnect(int index, pqxx::connection& conn, unsigned int attempt = 0, unsigned int max_attempts = 5) {
            try {
                logger->info("Reconnecting to database (conneciton {}, attempt {})...", index, attempt+1);
                if(conn.is_open()) {
                    conn.close();
                }
                conn = pqxx::connection(connection_string);
                conn.set_session_var("application_name", std::format("backend-worker-{}", index));
                prepare_statements(conn);
                logger->info("Reconnected to database (connection {}, attempt {})", index, attempt+1);
                return;
            } catch(const std::exception& e) {
                logger->error("Failed to reconnect to database (connection {}, attempt {}): {}", index, attempt+1, e.what());
            } catch(...) {
                logger->error("Failed to reconnect to database (connection {}, attempt {})", index, attempt+1);
            }

            if(attempt < max_attempts) {
                std::chrono::seconds backoff{1<<attempt};
                logger->info("Reconnecting in {} seconds...", backoff.count());
                std::this_thread::sleep_for(backoff);
                reconnect(index, conn, attempt + 1, max_attempts);
            } else {
                logger->critical("Failed to reconnect to database (connection {}) after {} attempts", index, max_attempts);
                std::terminate();
            }
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

                try {
                    work(conn);
                } catch(const pqxx::failure& failure) {
                    logger->error("pqxx failure in worker {}: {}", id, failure.what());
                    if(failure.poisons_connection()) {
                        logger->error("Connection of worker {} is poisoned. Reconnecting...");
                        reconnect(id, conn);
                    }
                } catch(const std::exception& ex) {
                    logger->error("Unhandled exception in worker {}: {}", id, ex.what());
                } catch(...) {
                    logger->error("Unhandled unknown exception in worker {}", id);
                }
                promise.set_value();
            }
        }

        std::string connection_string;
        std::shared_ptr<spdlog::logger> logger;
        std::vector<pqxx::connection> connections;
        std::vector<std::jthread> threads;
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::pair<std::move_only_function<void(pqxx::connection&)>, std::promise<void>>> queue;
};

}
