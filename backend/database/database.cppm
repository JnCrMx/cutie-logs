module;
#include <chrono>
#include <condition_variable>
#include <deque>
#include <format>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

export module backend.database;

import pqxx;
import spdlog;
import glaze;

import common;

namespace pqxx {
    template<> std::string const type_name<common::log_severity>{"log_severity"};
    template<> struct nullness<common::log_severity> : pqxx::no_null<common::log_severity> {};
    template<> struct string_traits<common::log_severity> {
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
        void insert_log(pqxx::transaction_base& txn, std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>& timestamp,
            const std::string& scope, common::log_severity severity, const glz::json_t& attributes, const glz::json_t& body)
        {
            std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double, std::chrono::seconds::period>> ts_seconds = timestamp;
            pqxx::result res = txn.exec(pqxx::prepped{"insert_log"}, {ts_seconds.time_since_epoch().count(), scope, severity, *attributes.dump(), *body.dump()});
        }
    private:
        void prepare_statements(pqxx::connection& conn) {
            conn.prepare("insert_log",
                "INSERT INTO logs (resource, timestamp, scope, severity, attributes, body) "
                "VALUES (1, to_timestamp($1::double precision), $2, $3, $4::jsonb, $5::jsonb)");
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
