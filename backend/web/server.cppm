module;

#include <memory>

export module backend.web;

import pistache;
import spdlog;
import backend.utils;
import backend.database;

namespace backend::web {
    export class Server {
        public:
            static Pistache::Address defaultAddress() {
                return Pistache::Address(Pistache::Ipv4::any(), Pistache::Port(8080));
            }
            static Pistache::Http::Endpoint::Options defaultOptions() {
                return Pistache::Http::Endpoint::options()
                    .threads(4)
                    .flags(Pistache::Tcp::Options::ReuseAddr);
            }

            Server(database::Database& db, Pistache::Address address = defaultAddress(), Pistache::Http::Endpoint::Options options = defaultOptions())
                : db(db), address(address), server(address), router(), logger(spdlog::default_logger()->clone("web"))
            {
                server.init(options);

                setupStaticRoutes();
                setupApiRoutes();

                server.setHandler(router.handler());
            }

            void serve() {
                logger->info("Serving web interface on http://{}", address);
                server.serve();
            }
            void serveThreaded() {
                logger->info("Serving web interface on http://{}", address);
                server.serveThreaded();
            }
        private:
            void setupStaticRoutes();
            void setupApiRoutes();

            std::shared_ptr<spdlog::logger> logger;
            Pistache::Address address;
            Pistache::Http::Endpoint server;
            Pistache::Rest::Router router;
            database::Database& db;
    };
}
