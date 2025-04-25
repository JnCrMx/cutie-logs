module;
#include <chrono>
#include <memory>
#include <unordered_set>

export module backend.opentelemetry;

import glaze;
import gzip;
import pistache;
import pqxx;
import proto;
import spdlog;

import common;
import backend.utils;
import backend.database;

glz::json_t to_json(const ::opentelemetry::proto::common::v1::AnyValue& v) {
    if(v.has_bool_value()) {
        return v.bool_value();
    } else if(v.has_int_value()) {
        return v.int_value();
    } else if(v.has_double_value()) {
        return v.double_value();
    } else if(v.has_string_value()) {
        std::string s = v.string_value();
        // remove non-printable characters
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return !std::isprint(c); }), s.end());
        return s;
    } else if(v.has_kvlist_value()) {
        auto obj = glz::json_t::object_t{};
        for(auto& kv : v.kvlist_value().values()) {
            obj[kv.key()] = to_json(kv.value());
        }
        return obj;
    } else if(v.has_array_value()) {
        auto arr = glz::json_t::array_t{};
        for(auto& elem : v.array_value().values()) {
            arr.push_back(to_json(elem));
        }
        return arr;
    }
    return glz::json_t::null_t{};
}
glz::json_t::object_t to_json(const ::google::protobuf::RepeatedPtrField<::opentelemetry::proto::common::v1::KeyValue>& kv) {
    auto obj = glz::json_t::object_t{};
    for(const auto& elem : kv) {
        obj[elem.key()] = to_json(elem.value());
    }
    return obj;
}

namespace backend::opentelemetry {
    export class Server {
        public:
            static Pistache::Address default_address() {
                return Pistache::Address(Pistache::Ipv4::any(), Pistache::Port(4318));
            }
            static Pistache::Http::Endpoint::Options default_options() {
                return Pistache::Http::Endpoint::options()
                    .threads(4)
                    .flags(Pistache::Tcp::Options::ReuseAddr);
            }

            Server(database::Database& db, Pistache::Address address = default_address(), Pistache::Http::Endpoint::Options options = default_options())
                : db(db), address(address), server(address), router(), logger(spdlog::default_logger()->clone("opentelemetry"))
            {
                server.init(options);

                router.post("/v1/logs", Pistache::Rest::Routes::bind(&Server::handle_log, this));
                server.setHandler(router.handler());
            }

            void serve() {
                logger->info("Serving OpenTelemetry collector on http://{}", address);
                server.serve();
            }
            void serve_threaded() {
                logger->info("Serving OpenTelemetry collector on http://{}", address);
                server.serveThreaded();
            }
        private:
            Pistache::Rest::Route::Result handle_log(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
                try {
                    logger->trace("{} | Received a POST request to /v1/logs", request.address());

                    if(request.body().empty()) {
                        logger->warn("{} | Empty request body", request.address());
                        response.send(Pistache::Http::Code::Bad_Request, "Empty request body");
                        return Pistache::Rest::Route::Result::Failure;
                    }
                    if(request.headers().get<Pistache::Http::Header::ContentType>()->mime().raw() != "application/x-protobuf") {
                        logger->warn("{} | Invalid Content-Type header", request.address());
                        response.send(Pistache::Http::Code::Unsupported_Media_Type, "Invalid Content-Type header");
                        return Pistache::Rest::Route::Result::Failure;
                    }

                    std::string body;
                    if(request.headers().has<Pistache::Http::Header::ContentEncoding>()) {
                        auto encoding = request.headers().get<Pistache::Http::Header::ContentEncoding>();
                        if(encoding->encoding() == Pistache::Http::Header::Encoding::Gzip) {
                            body = gzip::decompress(request.body().data(), request.body().size());
                        } else {
                            logger->warn("{} | Unsupported Content-Encoding: {}", request.address(), Pistache::Http::Header::encodingString(encoding->encoding()));
                            response.send(Pistache::Http::Code::Unsupported_Media_Type, "Unsupported Content-Encoding");
                            return Pistache::Rest::Route::Result::Failure;
                        }
                    } else {
                        body = request.body();
                    }

                    ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest req;
                    if(!req.ParseFromString(body)) {
                        logger->warn("{} | Failed to parse request body", request.address());
                        response.send(Pistache::Http::Code::Bad_Request, "Invalid request body");
                        return Pistache::Rest::Route::Result::Failure;
                    }

                    db.queue_work([this, address = request.address(), req = std::move(req), response = std::move(response)](pqxx::connection& conn) mutable {
                        try {
                            using timestamp_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
                            std::unordered_set<decltype(std::declval<timestamp_t>().time_since_epoch().count())> seen_timestamps;
                            static uint64_t timestamp_fix_offset = 0;

                            for(auto& resourceLog : req.resource_logs()) {
                                unsigned int resource = db.ensure_resource(conn, to_json(resourceLog.resource().attributes()));
                                for(auto& scopeLog : resourceLog.scope_logs()) {
                                    for(auto& log : scopeLog.log_records()) {
                                        std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> ts{std::chrono::nanoseconds(log.time_unix_nano())};
                                        // check if ts includes anything below seconds
                                        if(ts == std::chrono::floor<std::chrono::seconds>(ts)) {
                                            auto fixed_ts = ts + (std::chrono::microseconds(timestamp_fix_offset++) % std::chrono::seconds(1));
                                            logger->warn("{} | Adjusted second-precision timestamp from {} to {}", address, ts.time_since_epoch().count(), fixed_ts.time_since_epoch().count());
                                            ts = fixed_ts;
                                        }

                                        while(seen_timestamps.contains(ts.time_since_epoch().count())) {
                                            ts += std::chrono::microseconds(1);
                                        }
                                        seen_timestamps.insert(ts.time_since_epoch().count());

                                        glz::json_t::object_t attributes = to_json(log.attributes());
                                        common::log_severity severity = static_cast<common::log_severity>(log.severity_number());
                                        db.insert_log(conn, resource, ts, scopeLog.scope().name(), severity, attributes, to_json(log.body()));
                                    }
                                }
                            }
                            response.send(Pistache::Http::Code::Ok, "");
                        } catch(const std::exception& e) {
                            logger->error("{} | Unhandled exception: {} for request {}", address, e.what(), req.DebugString());
                            response.send(Pistache::Http::Code::Internal_Server_Error, "Internal server error");
                        }
                    });
                } catch(const std::exception& e) {
                    logger->error("{} | Unhandled exception: {}", request.address(), e.what());
                }
                return Pistache::Rest::Route::Result::Ok;
            }

            std::shared_ptr<spdlog::logger> logger;
            Pistache::Address address;
            Pistache::Http::Endpoint server;
            Pistache::Rest::Router router;
            database::Database& db;
    };
}
