module;
#include <filesystem>
#include <memory>
#include <optional>

export module backend.web;

import pistache;
import spdlog;
import backend.utils;
import backend.database;
import common;

namespace backend::web {
    export class Server {
        public:
            static Pistache::Address default_address() {
                return Pistache::Address(Pistache::Ipv4::any(), Pistache::Port(8080));
            }
            static Pistache::Http::Endpoint::Options default_options() {
                return Pistache::Http::Endpoint::options()
                    .threads(4)
                    .flags(Pistache::Tcp::Options::ReuseAddr);
            }

            Server(database::Database& db, common::shared_settings& settings, Pistache::Address address = default_address(), Pistache::Http::Endpoint::Options options = default_options())
                : db(db), settings(settings), address(address), server(address), router(), logger(spdlog::default_logger()->clone("web"))
            {
                server.init(options);

                setup_static_routes();
                setup_api_routes();

                server.setHandler(router.handler());
            }
            void set_static_dev_path(std::filesystem::path path) {
                static_dev_path = path;
            }

            void serve() {
                logger->info("Serving web interface on http://{}", address);
                server.serve();
            }
            void serve_threaded() {
                logger->info("Serving web interface on http://{}", address);
                server.serveThreaded();
            }
        private:
            void setup_static_routes();
            void setup_api_routes();

            std::shared_ptr<spdlog::logger> logger;
            Pistache::Address address;
            Pistache::Http::Endpoint server;
            Pistache::Rest::Router router;
            database::Database& db;

            common::shared_settings& settings;
            std::optional<std::filesystem::path> static_dev_path;
    };
}
