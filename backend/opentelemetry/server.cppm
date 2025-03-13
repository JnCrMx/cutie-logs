module;

#include <cstdint>
#include <iostream>
#include <format>

export module backend.opentelemetry;

import pistache;
import proto;
import gzip;

namespace backend::opentelemetry {
    export class Server {
        public:
            static Pistache::Address defaultAddress() {
                return Pistache::Address(Pistache::Ipv4::any(), Pistache::Port(4318));
            }
            static Pistache::Http::Endpoint::Options defaultOptions() {
                return Pistache::Http::Endpoint::options()
                    .threads(4)
                    .flags(Pistache::Tcp::Options::ReuseAddr);
            }

            Server(Pistache::Address address = defaultAddress(), Pistache::Http::Endpoint::Options options = defaultOptions())
                : address(address), server(address), router() {
                server.init(options);

                router.post("/v1/logs", Pistache::Rest::Routes::bind(&Server::handleLog, this));
                server.setHandler(router.handler());
            }

            void serve() {
                std::cout << std::format("Serving OpenTelemetry collector on http://{}:{}\n", address.host(), static_cast<uint16_t>(address.port()));
                server.serve();
            }
            void serveThreaded() {
                std::cout << std::format("Serving OpenTelemetry collector on http://{}:{}\n", address.host(), static_cast<uint16_t>(address.port()));
                server.serveThreaded();
            }
        private:
            Pistache::Rest::Route::Result handleLog(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
                std::cout << "Received a POST request to /v1/logs" << std::endl;
                std::string body = gzip::decompress(request.body().data(), request.body().size());

                ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest req;
                if(!req.ParseFromString(body)) {
                    std::cout << "Failed to parse request body" << std::endl;
                    response.send(Pistache::Http::Code::Bad_Request, "Invalid request body");
                    return Pistache::Rest::Route::Result::Failure;
                }

                for(auto& resourceLog : req.resource_logs()) {
                    for(auto& log : resourceLog.scope_logs()) {
                        std::cout << "Log: " << log.scope().name() << std::endl;
                    }
                }

                response.send(Pistache::Http::Code::Ok, "Hello, World! :D");
                return Pistache::Rest::Route::Result::Ok;
            }

            Pistache::Address address;
            Pistache::Http::Endpoint server;
            Pistache::Rest::Router router;
    };
}
