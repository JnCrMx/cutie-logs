module;
#include <condition_variable>
#include <format>
#include <string>
#include <thread>
#include <vector>

export module backend.database;

import pqxx;
import spdlog;

namespace backend::database {

export class database {
    public:
        constexpr static unsigned int default_connection_count = 4;

        database(const std::string& connection_string, unsigned int connection_count = default_connection_count)
            : logger(spdlog::default_logger()->clone("database"))
        {
            connections.reserve(connection_count);
            for(int i = 0; i < connection_count; i++) {
                connections.emplace_back(connection_string);
                auto& conn = connections.back();
                conn.set_session_var("application_name", std::format("backend-{}", i));

                auto& thread = threads.emplace_back([this, i, &conn](std::stop_token st) {
                    worker(i, conn, st);
                });
                pthread_setname_np(thread.native_handle(), std::format("backend-{}", i).c_str());
            }
        }

    private:
        void worker(unsigned int id, pqxx::connection& conn, std::stop_token st) {
            logger->debug("Worker {} started", id);
        }

        std::shared_ptr<spdlog::logger> logger;
        std::vector<pqxx::connection> connections;
        std::vector<std::jthread> threads;
        std::condition_variable cv;
};

}
