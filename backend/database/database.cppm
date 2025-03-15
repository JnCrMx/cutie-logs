module;
#include <condition_variable>
#include <deque>
#include <format>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

export module backend.database;

import pqxx;
import spdlog;

namespace backend::database {

export class Database {
    public:
        constexpr static unsigned int default_connection_count = 4;

        Database(const std::string& connection_string, unsigned int connection_count = default_connection_count)
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

        void queue_work(std::function<void(pqxx::connection&)> work) {
            std::unique_lock lock(mutex);
            queue.push_back(work);
            cv.notify_one();
        }

        void run_migrations();
    private:
        void worker(unsigned int id, pqxx::connection& conn, std::stop_token st) {
            logger->debug("Worker {} started", id);

            while(!st.stop_requested()) {
                std::function<void(pqxx::connection&)> work;
                {
                    std::unique_lock lock(mutex);
                    cv.wait(lock, [this, &work, &st] {
                        if(st.stop_requested()) {
                            return true;
                        }

                        if(queue.empty()) {
                            return false;
                        }

                        work = queue.front();
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
        std::deque<std::function<void(pqxx::connection&)>> queue;
};

}
