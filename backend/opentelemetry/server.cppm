module;
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <unordered_set>

export module backend.opentelemetry;

import glaze;
import gzip;
import pqxx;
import proto;
import spdlog;

import common;
import backend.utils;
import backend.database;
import backend.notifications;

namespace mime {

constexpr auto text_plain = "text/plain";

}

glz::generic to_json(const ::opentelemetry::proto::common::v1::AnyValue& v) {
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
        auto obj = glz::generic::object_t{};
        for(auto& kv : v.kvlist_value().values()) {
            obj[kv.key()] = to_json(kv.value());
        }
        return obj;
    } else if(v.has_array_value()) {
        auto arr = glz::generic::array_t{};
        for(auto& elem : v.array_value().values()) {
            arr.push_back(to_json(elem));
        }
        return arr;
    } else if(v.has_bytes_value()) {
        auto arr = glz::generic::array_t{};
        for(char c : v.bytes_value()) {
            arr.push_back(static_cast<unsigned char>(c));
        }
        return arr;
    }
    return glz::generic::null_t{};
}
glz::generic::object_t to_json(const ::google::protobuf::RepeatedPtrField<::opentelemetry::proto::common::v1::KeyValue>& kv) {
    auto obj = glz::generic::object_t{};
    for(const auto& elem : kv) {
        obj[elem.key()] = to_json(elem.value());
    }
    return obj;
}

inline std::future<void> make_ready_future() {
    std::promise<void> p;
    p.set_value();
    return p.get_future();
}
using common::maybe;

namespace backend::opentelemetry {
    export class Server {
        public:
            Server(asio::io_context& ctx, database::Database& db, NetworkIpFilter* ip_filter, asio::ip::tcp::endpoint endpoint)
                : db(db), ip_filter(ip_filter), endpoint(endpoint), server(ctx.get_executor()), logger(spdlog::default_logger()->clone("opentelemetry"))
            {
                router.post_async("/v1/logs", std::bind_front(&Server::handle_log, this));
                server.mount("/", router);

                auto f = db.queue_work([this](pqxx::connection& conn) {
                    {
                        pqxx::nontransaction txn(conn);
                        alert_rules = this->db.get_alert_rules(txn);
                    }

                    conn.listen("alert_rules", [this](pqxx::notification notification){
                        pqxx::nontransaction txn(notification.conn);
                        alert_rules = this->db.get_alert_rules(txn);
                        logger->info("Reloaded {} alert rule(s)", alert_rules.size());
                    });
                });
                f.wait();
                logger->info("Loaded {} alert rule(s)", alert_rules.size());
            }

            void start() {
                logger->info("Serving OpenTelemetry collector on http://{}:{}/", endpoint.address().to_string(), endpoint.port());
                server.bind(endpoint.address().to_string(), endpoint.port());
                server.start(0);
            }
        private:
            void process_alerts(pqxx::connection& conn, const common::log_entry& log, const common::log_resource& resource) {
                pqxx::work txn(conn);
                for(const auto& [_, rule] : alert_rules) {
                    if(!rule.match(log)) {
                        continue;
                    }

                    logger->trace("Sending alert for rule {}:{}", rule.id, rule.name);
                    common::alert_stencil_object msg{
                        .rule = &rule,
                        .resource = &resource,
                        .log = &log
                    };

                    auto provider = notifications::registry::instance()
                        .create_provider(rule.notification_provider, *logger, rule.notification_options);
                    if(!provider) {
                        logger->error("Failed to create notification provider {} for rule {}:{}: {}",
                            rule.notification_provider, rule.id, rule.name, provider.error().message);
                        txn.exec(pqxx::prepped{"update_alert_result"}, pqxx::params{rule.id, false, provider.error().message});
                        continue;
                    }
                    auto result = (*provider)->notify(*logger, msg, ip_filter);
                    if(!result) {
                        logger->error("Failed to send notification for rule {}:{}: {}",
                            rule.id, rule.name, result.error().message);
                        txn.exec(pqxx::prepped{"update_alert_result"}, pqxx::params{rule.id, false, result.error().message});
                        continue;
                    }
                    txn.exec(pqxx::prepped{"update_alert_result"}, pqxx::params{rule.id, true, std::nullopt});
                }
                txn.commit();
            }

            std::future<void> handle_log(const glz::request& request, glz::response& response) {
                std::string remote = request.remote_ip + ":" + std::to_string(request.remote_port);
                try {
                    logger->trace("{} | Received a POST request to /v1/logs", remote);

                    if(request.body.empty()) {
                        logger->warn("{} | Empty request body", remote);
                        response.status(400).content_type(mime::text_plain).body("Empty request body");
                        return make_ready_future();
                    }
                    if(maybe(request.headers, "content-type") != "application/x-protobuf") {
                        logger->warn("{} | Invalid Content-Type header", remote);
                        response.status(415).content_type(mime::text_plain).body("Invalid Content-Type header");
                        return make_ready_future();
                    }

                    std::string body;
                    if(auto encoding = maybe(request.headers, "content-encoding")) {
                        if(*encoding == "gzip") {
                            body = gzip::decompress(request.body.data(), request.body.size());
                        } else {
                            logger->warn("{} | Unsupported Content-Encoding: {}", remote, *encoding);
                            response.status(415).content_type(mime::text_plain).body("Unsupported Content-Encoding");
                            return make_ready_future();
                        }
                    } else {
                        body = request.body;
                    }

                    ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest req;
                    if(!req.ParseFromString(body)) {
                        logger->warn("{} | Failed to parse request body", remote);
                        response.status(400).content_type(mime::text_plain).body("Invalid request body");
                        return make_ready_future();
                    }

                    return db.queue_work([this, remote = std::move(remote), req = std::move(req), &response](pqxx::connection& conn) {
                        try {
                            using timestamp_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
                            std::unordered_set<decltype(std::declval<timestamp_t>().time_since_epoch().count())> seen_timestamps;
                            static uint64_t timestamp_fix_offset = 0;

                            for(auto& resourceLog : req.resource_logs()) {
                                glz::generic::object_t resource_attributes = to_json(resourceLog.resource().attributes());
                                unsigned int resource = db.ensure_resource(conn, resource_attributes);
                                common::log_resource log_resource{
                                    .id = resource,
                                    .attributes = std::move(resource_attributes),
                                };

                                for(auto& scopeLog : resourceLog.scope_logs()) {
                                    for(auto& log : scopeLog.log_records()) {
                                        std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> ts{std::chrono::nanoseconds(log.time_unix_nano())};
                                        // check if ts includes anything below seconds
                                        if(ts == std::chrono::floor<std::chrono::seconds>(ts)) {
                                            auto fixed_ts = ts + (std::chrono::microseconds(timestamp_fix_offset++) % std::chrono::seconds(1));
                                            logger->warn("{} | Adjusted second-precision timestamp from {} to {}", remote, ts.time_since_epoch().count(), fixed_ts.time_since_epoch().count());
                                            ts = fixed_ts;
                                        }

                                        while(seen_timestamps.contains(ts.time_since_epoch().count())) {
                                            ts += std::chrono::microseconds(1);
                                        }
                                        seen_timestamps.insert(ts.time_since_epoch().count());

                                        glz::generic::object_t attributes = to_json(log.attributes());
                                        glz::generic body = to_json(log.body());
                                        common::log_severity severity = static_cast<common::log_severity>(log.severity_number());
                                        db.insert_log(conn, resource, ts, scopeLog.scope().name(), severity, attributes, body);

                                        common::log_entry log_entry{
                                            .resource = resource,
                                            .timestamp = std::chrono::time_point_cast<std::chrono::duration<double>>(ts).time_since_epoch().count(),
                                            .scope = scopeLog.scope().name(),
                                            .severity = severity,
                                            .attributes = std::move(attributes),
                                            .body = std::move(body)
                                        };
                                        process_alerts(conn, log_entry, log_resource);
                                    }
                                }
                            }
                            response.status(200).body("");
                        } catch(const std::exception& e) {
                            logger->error("{} | Unhandled exception: {} for request {}", remote, e.what(), req.DebugString());
                            response.status(500).content_type(mime::text_plain).body(e.what());
                        }
                    });
                } catch(const std::exception& e) {
                    logger->error("{} | Unhandled exception: {}", remote, e.what());
                }
                return make_ready_future();
            }

            std::shared_ptr<spdlog::logger> logger;
            asio::ip::tcp::endpoint endpoint;
            glz::http_server<false> server;
            glz::http_router router;
            database::Database& db;
            NetworkIpFilter* ip_filter;

            std::map<unsigned int, common::alert_rule> alert_rules;
    };
}
