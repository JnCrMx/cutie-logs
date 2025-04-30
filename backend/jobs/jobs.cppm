module;
#include <functional>
#include <memory>
#include <thread>

export module backend.jobs;

import spdlog;

import backend.database;

namespace backend::jobs {

export class Jobs {
    public:
        Jobs(database::Database& db)
            : db(db), logger(spdlog::default_logger()->clone("jobs"))
        {

        }

        void start() {
            logger->info("Starting jobs thread");
            thread = std::jthread(std::bind_front(&Jobs::job_thread, this));
        }
    private:
        static constexpr auto job_interval = std::chrono::minutes(1);

        void job_thread(std::stop_token st) {
            std::this_thread::sleep_for(std::chrono::seconds(10));

            while(!st.stop_requested()) {
                try {
                    run_cleanup_jobs();

                    std::this_thread::sleep_for(job_interval);
                } catch(const std::exception& e) {
                    logger->error("Error in jobs thread: {}", e.what());
                }
            }
        }

        void run_cleanup_jobs();

        std::jthread thread;
        std::shared_ptr<spdlog::logger> logger;
        database::Database& db;
};

}
