module;

#include <cstdint>
#include <iostream>
#include <format>

export module backend.web;

import pistache;

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

            Server(Pistache::Address address = defaultAddress(), Pistache::Http::Endpoint::Options options = defaultOptions())
                : address(address), server(address), router() {
                server.init(options);

                setupStaticRoutes();
                setupApiRoutes();

                server.setHandler(router.handler());
            }

            void serve() {
                std::cout << std::format("Serving web interface on http://{}:{}\n", address.host(), static_cast<uint16_t>(address.port()));
                server.serve();
            }
            void serveThreaded() {
                std::cout << std::format("Serving web interface on http://{}:{}\n", address.host(), static_cast<uint16_t>(address.port()));
                server.serveThreaded();
            }
        private:
            void setupStaticRoutes();
            void setupApiRoutes();

            Pistache::Address address;
            Pistache::Http::Endpoint server;
            Pistache::Rest::Router router;
    };
}
