module;
#include <filesystem>
#include <memory>
#include <optional>

export module backend.web;

import asio;
import glaze;
import spdlog;
import backend.utils;
import backend.database;
import common;

namespace backend::web {
    export class Server {
        public:
            Server(asio::io_context& ctx, database::Database& db, common::shared_settings& settings, asio::ip::tcp::endpoint endpoint)
                : db(db), settings(settings), endpoint(endpoint), server(ctx.get_executor()), logger(spdlog::default_logger()->clone("web"))
            {
                setup_static_routes();
                setup_api_routes();
                server.mount("/", router);
            }
            void set_static_dev_path(std::filesystem::path path) {
                static_dev_path = path;
            }

            void start() {
                logger->info("Serving web interface on http://{}:{}/", endpoint.address().to_string(), endpoint.port());
                server.bind(endpoint.address().to_string(), endpoint.port());
                server.start(0);
            }
        private:
            void setup_static_routes();
            void setup_api_routes();

            std::shared_ptr<spdlog::logger> logger;
            asio::ip::tcp::endpoint endpoint;
            glz::http_server<false> server;
            glz::http_router router;

            database::Database& db;

            common::shared_settings& settings;
            std::optional<std::filesystem::path> static_dev_path;
    };
}
