module;
#include <chrono>
#include <condition_variable>
#include <deque>
#include <format>
#include <functional>
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

        void start_workers() {
            for(int i = 0; i < connections.size(); i++) {
                auto& thread = threads.emplace_back([this, i](std::stop_token st) {
                    worker(i, connections[i], st);
                });
                pthread_setname_np(thread.native_handle(), std::format("backend-worker-{}", i).c_str());
            }
        }

        void queue_work(std::move_only_function<void(pqxx::connection&)>&& work) {
            std::unique_lock lock(mutex);
            queue.push_back(std::move(work));
            cv.notify_one();
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
        }

        void worker(unsigned int id, pqxx::connection& conn, std::stop_token st) {
            logger->debug("Worker {} started", id);
            prepare_statements(conn);

            while(!st.stop_requested()) {
                std::move_only_function<void(pqxx::connection&)> work;
                {
                    std::unique_lock lock(mutex);
                    cv.wait(lock, [this, &work, &st] {
                        if(st.stop_requested()) {
                            return true;
                        }

                        if(queue.empty()) {
                            return false;
                        }

                        work = std::move(queue.front());
                        queue.pop_front();
                        return true;
                    });
                }

                if(st.stop_requested()) {
                    break;
                }

                work(conn);
            }
        }

        std::shared_ptr<spdlog::logger> logger;
        std::vector<pqxx::connection> connections;
        std::vector<std::jthread> threads;
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::move_only_function<void(pqxx::connection&)>> queue;
};

}
