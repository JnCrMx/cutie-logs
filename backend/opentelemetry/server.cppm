module;

#include <cstdint>
#include <iostream>
#include <memory>

export module backend.opentelemetry;

import pistache;
import proto;
import gzip;
import spdlog;
import backend.utils;

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
                : address(address), server(address), router(), logger(spdlog::default_logger()->clone("opentelemetry")) {
                server.init(options);

                router.post("/v1/logs", Pistache::Rest::Routes::bind(&Server::handleLog, this));
                server.setHandler(router.handler());
            }

            void serve() {
                logger->info("Serving OpenTelemetry collector on http://{}", address);
                server.serve();
            }
            void serveThreaded() {
                logger->info("Serving OpenTelemetry collector on http://{}", address);
                server.serveThreaded();
            }
        private:
            Pistache::Rest::Route::Result handleLog(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
                logger->trace("{} | Received a POST request to /v1/logs", request.address());
                std::string body = gzip::decompress(request.body().data(), request.body().size());

                ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest req;
                if(!req.ParseFromString(body)) {
                    logger->warn("{} | Failed to parse request body", request.address());
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

            std::shared_ptr<spdlog::logger> logger;
            Pistache::Address address;
            Pistache::Http::Endpoint server;
            Pistache::Rest::Router router;
    };
}
