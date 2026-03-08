module;
#include <algorithm>
#include <chrono>
#include <expected>
#include <format>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <malloc.h>

module backend.web;

import glaze;
import pqxx;

import common;

namespace backend::web {

namespace mime {
static const auto application_json = Pistache::Http::Mime::MediaType{Pistache::Http::Mime::Type::Application, Pistache::Http::Mime::Subtype::Json};
static const auto application_octet = Pistache::Http::Mime::MediaType{Pistache::Http::Mime::Type::Application, Pistache::Http::Mime::Subtype::OctetStream};
static const auto application_beve = Pistache::Http::Mime::MediaType{"application/prs.beve", Pistache::Http::Mime::MediaType::DoParse};
static const auto application_ndjson = Pistache::Http::Mime::MediaType{"application/x-ndjson", Pistache::Http::Mime::MediaType::DoParse};
}

bool mime_equals(const Pistache::Http::Mime::MediaType& lhs, const Pistache::Http::Mime::MediaType& rhs) {
    if(lhs.top() != rhs.top()) {
        return false;
    }
    if(lhs.sub() != rhs.sub()) {
        return false;
    }
    if(lhs.suffix() != rhs.suffix()) {
        return false;
    }
    if(lhs.sub() == Pistache::Http::Mime::Subtype::Vendor || lhs.sub() == Pistache::Http::Mime::Subtype::Ext) {
        return lhs.rawSub() == rhs.rawSub();
    }
    return true;
}

bool accepts(const Pistache::Http::Request& req, Pistache::Http::Mime::MediaType type) {
    if(!req.headers().has<Pistache::Http::Header::Accept>()) {
        return false;
    }
    auto accept = req.headers().get<Pistache::Http::Header::Accept>()->media();
    return std::ranges::find_if(accept, [&type](const auto& mime) {
        return mime_equals(mime, type);
    }) != accept.end();
}
bool isContentType(const Pistache::Http::Request& req, Pistache::Http::Mime::MediaType type) {
    if(!req.headers().has<Pistache::Http::Header::ContentType>()) {
        return false;
    }
    auto content_type = req.headers().get<Pistache::Http::Header::ContentType>()->mime();
    return mime_equals(content_type, type);
}

template<typename T>
void send_response(Pistache::Http::ResponseWriter& response, bool beve, const T& data) {
    auto res_data = beve ? *glz::write<common::beve_opts>(data) : *glz::write<common::json_opts>(data);
    response.send(Pistache::Http::Code::Ok, res_data.data(), res_data.size(),
        beve ? mime::application_beve : mime::application_json);
}

template<typename T>
void stream_response(Pistache::Http::ResponseStream& stream, bool beve, const T& data, bool first) {
    if(beve) {
        std::string buffer{};
        if(first) {
            [[maybe_unused]] auto _ = glz::write_beve_append<common::beve_opts>(data, buffer);
        } else {
            [[maybe_unused]] auto _ = glz::write_beve_append_with_delimiter<common::beve_opts>(data, buffer);
        }
        stream.write(buffer.data(), buffer.size());
    } else {
        const char newline = '\n';
        if(!first) {
            stream.write(&newline, 1);
        }
        auto res_data = *glz::write<common::json_opts>(data);
        stream.write(res_data.data(), res_data.size());
    }
}

struct query_parameters {
    struct all_attributes{};

    unsigned int limit = 100;
    unsigned int offset = 0;
    std::optional<std::variant<std::vector<std::string>, all_attributes>> attributes; // NOT escaped yet!
    std::vector<std::string> scopes; // NOT escaped yet!
    std::vector<unsigned int> resources;
};
query_parameters parse_parameters(const Pistache::Rest::Request& request) {
    constexpr auto url_decode = [](std::string_view sv) -> std::string { return glz::url_decode(sv); };
    auto attributes = request.query().get("attributes").transform(url_decode);
    auto scopes = request.query().get("scopes").transform(url_decode);
    auto resources = request.query().get("resources").transform(url_decode);
    auto limit = request.query().get("limit").transform(url_decode);
    auto offset = request.query().get("offset").transform(url_decode);

    query_parameters params{};
    if(attributes) {
        if(attributes == "*") {
            params.attributes = query_parameters::all_attributes{};
        } else {
            auto& v = std::get<std::vector<std::string>>(*(params.attributes = std::vector<std::string>{}));
            for(const auto& a : *attributes | std::views::split(',')) {
                std::string_view sv{a};
                v.emplace_back(sv);
            }
        }
    }
    if(scopes) {
        for(const auto& s : *scopes | std::views::split(',')) {
            std::string_view sv{s};
            if(sv == "<empty>") {
                params.scopes.emplace_back("");
            } else {
                params.scopes.emplace_back(sv);
            }
        }
    }
    if(resources) {
        for(const auto& r : *resources | std::views::split(',')) {
            unsigned int ri{};
            if(std::from_chars(r.data(), r.data() + r.size(), ri).ec == std::errc{}) {
                params.resources.push_back(ri);
            }
        }
    }
    if(limit) {
        unsigned int li{};
        if(std::from_chars(limit->data(), limit->data() + limit->size(), li).ec == std::errc{}) {
            params.limit = li;
        }
    }
    if(offset) {
        unsigned int oi{};
        if(std::from_chars(offset->data(), offset->data() + offset->size(), oi).ec == std::errc{}) {
            params.offset = oi;
        }
    }
    return params;
}
std::expected<void, std::string> validate_parameters(const query_parameters& params, bool needs_attributes, bool streaming) {
    if(needs_attributes && !params.attributes) {
        return std::unexpected("not attributes parameter given");
    }

    auto limit = streaming ? max_query_limit_streaming : max_query_limit;
    if(params.limit > limit) {
        return std::unexpected(std::format("maximum query limit of {} exceeded", limit));
    }

    return std::expected<void, std::string>{};
}
std::expected<query_parameters, std::string> validate_parameters(const Pistache::Rest::Request& request, bool needs_attributes, bool streaming) {
    auto params = parse_parameters(request);
    auto e = validate_parameters(params, needs_attributes, streaming);
    return e.transform([&](){
        return std::move(params);
    });
}

std::string build_scope_filter(pqxx::transaction_base& txn, const std::vector<std::string>& scopes) {
    std::string filter = "scope IN (";
    for(const auto& scope : scopes) {
        filter += "'" + txn.esc(scope) + "',";
    }
    filter.pop_back();
    filter += ")";
    return filter;
}
std::string build_resource_filter(const std::vector<unsigned int>& resources) {
    std::string filter = "resource IN (";
    for(const auto& resource : resources) {
        filter += std::to_string(resource) + ",";
    }
    filter.pop_back();
    filter += ")";
    return filter;
}

std::string build_query(pqxx::transaction_base& txn, const query_parameters& params) {
    std::string query = "SELECT resource, extract(epoch from timestamp) as unix_time, scope, severity, body";
    if(std::holds_alternative<query_parameters::all_attributes>(*params.attributes)) {
        query += ", attributes";
    } else {
        for(const auto& attr : std::get<std::vector<std::string>>(*params.attributes)) {
            query += ", attributes->'";
            query += txn.esc(std::string_view{attr});
            query += "'";
        }
    }
    query += " FROM logs";
    query += " WHERE TRUE";
    if(!params.scopes.empty()) {
        query += " AND " + build_scope_filter(txn, params.scopes);
    }
    if(!params.resources.empty()) {
        query += " AND " + build_resource_filter(params.resources);
    }
    query += " ORDER BY timestamp DESC";
    query += " LIMIT " + std::to_string(params.limit);
    query += " OFFSET " + std::to_string(params.offset);
    return query;
}

std::vector<common::log_entry> get_logs(pqxx::transaction_base& txn, const query_parameters& params) {
    std::string query = build_query(txn, params);

    auto result = txn.exec(query);
    std::vector<common::log_entry> logs;
    logs.reserve(result.size());
    for(const auto& row : result) {
        auto& l = logs.emplace_back(
            row["resource"].as<unsigned int>(),
            row["unix_time"].as<double>(),
            row["scope"].as<std::string>(),
            row["severity"].as<common::log_severity>()
        );
        l.body = row["body"].as<std::optional<glz::generic>>().value_or(glz::generic::null_t{});
        unsigned int i=row.column_number("body")+1;
        if(std::holds_alternative<query_parameters::all_attributes>(*params.attributes)) {
            l.attributes = row["attributes"].as<glz::generic>();
        } else {
            for(const auto& attr : std::get<std::vector<std::string>>(*params.attributes)) {
                auto json = row[i].as<std::optional<glz::generic>>();
                if(json) {
                    l.attributes[attr] = std::move(*json);
                }
                i++;
            }
        }
    }
    return logs;
}

void stream_logs_all_attributes(pqxx::transaction_base& txn, const query_parameters& params, std::invocable<const common::log_entry&, unsigned int> auto&& consumer) {
    if(!std::holds_alternative<query_parameters::all_attributes>(*params.attributes)) {
        throw std::invalid_argument("params.attributes MUST be query_parameters::all_attributes");
    }

    std::string query = build_query(txn, params);
    unsigned int row_index = 0;
    for(const auto& [resource, timestamp, scope, severity, body, attributes] :
        txn.stream<unsigned int, double, std::string, common::log_severity, glz::generic, glz::generic>(query))
    {
        common::log_entry log{resource, timestamp, scope, severity, attributes, body};
        consumer(log, row_index++);
    }
}

namespace detail {
    template<typename ... T>
    using tuple_cat_t = decltype(std::tuple_cat(std::declval<T>()...));

    template<typename T, typename Seq>
    struct expander;

    template<typename T, std::size_t... Is>
    struct expander<T, std::index_sequence<Is...>> {
        template<typename E, std::size_t>
        using elem = E;
        using type = std::tuple<elem<T, Is>...>;
    };
    template <class Type, std::size_t N>
    struct tuple_N {
        using type = typename expander<Type, std::make_index_sequence<N>>::type;
    };

    template<typename Tuple>
    struct stream_helper;

    template<typename... Args>
    struct stream_helper<std::tuple<Args...>> {
        static auto stream(pqxx::transaction_base& txn, std::string_view query) {
            return txn.stream<Args...>(query);
        }
    };

    template<std::size_t... Is>
    void assign_attributes(common::log_entry& log, const auto& fields, const std::vector<std::string>& attr_names, std::index_sequence<Is...>) {
        (..., (log.attributes[attr_names[Is]] = std::get<5 + Is>(fields)));
    }

    template<std::size_t N, std::size_t MAX = 25>
    void stream_logs_N(pqxx::transaction_base& txn, const query_parameters& params, std::invocable<const common::log_entry&, unsigned int> auto&& consumer) {
        const auto& attributes = std::get<std::vector<std::string>>(*params.attributes);
        if(N < attributes.size()) {
            if constexpr (N >= MAX) { // at this point, just query all attributes
                return stream_logs_all_attributes(txn, params, std::move(consumer));
            } else {
                return stream_logs_N<N+1>(txn, params, std::move(consumer));
            }
        }

        std::string query = build_query(txn, params);

        using base_tuple = std::tuple<unsigned int, double, std::string, common::log_severity, glz::generic>;
        using attributes_tuple = tuple_N<glz::generic, N>::type;
        using tuple = tuple_cat_t<base_tuple, attributes_tuple>;

        auto stream = stream_helper<tuple>::stream(txn, query);
        unsigned int row_index = 0;
        for(const auto& fields : stream) {
            common::log_entry log{};
            log.resource = std::get<0>(fields);
            log.timestamp = std::get<1>(fields);
            log.scope = std::get<2>(fields);
            log.severity = std::get<3>(fields);
            log.body = std::get<4>(fields);
            assign_attributes(log, fields, attributes, std::make_index_sequence<N>{});

            consumer(log, row_index++);
        }
    }
}

void stream_logs(pqxx::transaction_base& txn, const query_parameters& params, std::invocable<const common::log_entry&, unsigned int> auto&& consumer) {
    if(std::holds_alternative<query_parameters::all_attributes>(*params.attributes)) {
        return stream_logs_all_attributes(txn, params, std::move(consumer));
    } else {
        return detail::stream_logs_N<0>(txn, params, std::move(consumer));
    }
}

std::expected<void, std::string> check_cleanup_rule(const common::cleanup_rule& rule) {
    if(rule.name.empty()) {
        return std::unexpected{"Field \"name\" cannot be empty"};
    }
    if(rule.execution_interval == std::chrono::seconds{0}) {
        return std::unexpected{"Field \"execution_interval\" cannot be 0s"};
    }
    if(rule.filter_minimum_age == std::chrono::seconds{0}) {
        return std::unexpected{"Field \"filter_minimum_age\" cannot be 0s"};
    }
    return {};
}
std::expected<void, std::string> check_alert_rule(const common::alert_rule& rule) {
    if(rule.name.empty()) {
        return std::unexpected{"Field \"name\" cannot be empty"};
    }
    if(rule.notification_provider.empty()) {
        return std::unexpected{"Field \"notification_provider\" cannot be empty"};
    }
    return {};
}

void Server::setup_api_routes() {
    router.get("/api/v1/healthz", [](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        response.send(Pistache::Http::Code::Ok, "OK");
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/version", [](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        response.send(Pistache::Http::Code::Ok, common::project_version.data(), common::project_version.size());
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/settings", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        bool accepts_beve = accepts(request, mime::application_beve);
        send_response(response, accepts_beve, settings);
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/logs", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        bool accepts_beve = accepts(request, mime::application_beve);
        bool accepts_ndjson = accepts(request, mime::application_ndjson);
        bool streaming = accepts_beve || accepts_ndjson; // we can stream both BEVE and NDJSON
        auto params = validate_parameters(request, true, streaming);
        if(!params) {
            response.send(Pistache::Http::Code::Bad_Request, params.error());
            return Pistache::Rest::Route::Result::Ok;
        }
        db.queue_work([this, accepts_beve, streaming, response = std::move(response), params = std::move(*params)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            try {
                if(streaming) {
                    auto stream = response.stream(Pistache::Http::Code::Ok);
                    stream_logs(txn, params, [&](const common::log_entry& entry, unsigned int row_index){
                        stream_response(stream, accepts_beve, entry, row_index == 0);
                    });
                    stream.ends();
                } else {
                    common::logs_response res = get_logs(txn, params);
                    send_response(response, false, res);
                }
            } catch(const pqxx::sql_error& e) {
                response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
            }
            malloc_trim(1024*1024);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/logs/stencil", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        constexpr auto url_decode = [](std::string_view sv) -> std::string { return glz::url_decode(sv); };
        auto params = validate_parameters(request, false, true /* /stencil always streams */);
        if(!params) {
            response.send(Pistache::Http::Code::Bad_Request, params.error());
            return Pistache::Rest::Route::Result::Ok;
        }
        auto stencil = request.query().get("stencil").transform(url_decode).value_or("");
        db.queue_work([this, response = std::move(response), params = std::move(*params), stencil = std::move(stencil)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            try {
                auto result = txn.exec(pqxx::prepped{"get_resources"});
                std::unordered_map<unsigned int, common::log_resource> resources;
                for(const auto& row : result) {
                    unsigned int id = row["id"].as<unsigned int>();
                    unsigned int count = row["count"].as<unsigned int>();
                    common::log_resource& r = resources[id];
                    r.id = id;
                    r.attributes = row["attributes"].as<glz::generic>();
                    r.created_at = row["created_at"].as<double>();
                }

                params.attributes = query_parameters::all_attributes{};

                auto stream = response.stream(Pistache::Http::Code::Ok);
                stream_logs_all_attributes(txn, params, [&](const common::log_entry& entry, unsigned int row_index) {
                    auto obj = common::log_entry_stencil_object::create(entry, resources);
                    std::string line = *common::stencil(stencil, obj)
                        .or_else([](auto err) -> std::expected<std::string, std::string> { return std::format("Stencil invalid: \"{}\"", err); })
                        +"\n";
                    stream.write(line.data(), line.size());
                });
                stream.ends();
            } catch(const pqxx::sql_error& e) {
                response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
            }
            malloc_trim(1024*1024);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/logs/attributes", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        bool accepts_beve = accepts(request, mime::application_beve);
        db.queue_work([this, accepts_beve, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            auto result = txn.exec(pqxx::prepped{"get_attributes"});
            common::logs_attributes_response res;
            for(const auto& row : result) {
                res.attributes[row["attribute"].as<std::string>()] = row["count"].as<int>();
            }
            res.total_logs = txn.exec(pqxx::prepped{"get_count"}).one_field().as<unsigned int>();

            send_response(response, accepts_beve, res);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/logs/scopes", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        bool accepts_beve = accepts(request, mime::application_beve);
        db.queue_work([this, accepts_beve, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            auto result = txn.exec(pqxx::prepped{"get_scopes"});
            common::logs_scopes_response res{};
            for(const auto& row : result) {
                int count = row["count"].as<int>();
                res.scopes[row["scope"].as<std::string>()] = count;
                res.total_logs += count; // we can avoid executing get_count query
            }

            send_response(response, accepts_beve, res);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/logs/resources", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        bool accepts_beve = accepts(request, mime::application_beve);
        db.queue_work([this, accepts_beve, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            auto result = txn.exec(pqxx::prepped{"get_resources"});
            common::logs_resources_response res;
            for(const auto& row : result) {
                unsigned int id = row["id"].as<unsigned int>();
                unsigned int count = row["count"].as<unsigned int>();
                common::log_resource r;
                r.id = id;
                r.attributes = row["attributes"].as<glz::generic>();
                r.created_at = row["created_at"].as<double>();
                res.resources[id] = {r, count};
            }

            send_response(response, accepts_beve, res);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/settings/cleanup_rules", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        bool accepts_beve = accepts(request, mime::application_beve);
        db.queue_work([this, accepts_beve, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};

            common::cleanup_rules_response res{db.get_cleanup_rules(txn)};
            send_response(response, accepts_beve, res);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    auto create_or_update_cleanup_rule = [this]<bool update>(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        bool accepts_beve = accepts(request, mime::application_beve);

        std::expected<common::cleanup_rule, glz::error_ctx> rule;
        if(isContentType(request, mime::application_json)) {
            rule = glz::read<common::json_opts, common::cleanup_rule>(request.body());
        } else if(isContentType(request, mime::application_beve)) {
            rule = glz::read<common::beve_opts, common::cleanup_rule>(request.body());
        } else {
            response.send(Pistache::Http::Code::Unsupported_Media_Type, "Unsupported media type");
            return Pistache::Rest::Route::Result::Ok;
        }
        if(!rule) {
            response.send(Pistache::Http::Code::Bad_Request, std::format("Failed to parse request body: {}", glz::format_error(rule.error(), request.body())));
            return Pistache::Rest::Route::Result::Ok;
        }

        if(auto res = check_cleanup_rule(*rule); !res) {
            response.send(Pistache::Http::Code::Bad_Request, std::format("Invalid request body: {}", res.error()));
            return Pistache::Rest::Route::Result::Ok;
        }

        unsigned int id = 0;
        if constexpr (update) {
            id = request.param(":id").as<unsigned int>();
            if(id != rule->id) {
                response.send(Pistache::Http::Code::Bad_Request, "ID in URL and body do not match");
                return Pistache::Rest::Route::Result::Ok;
            }
        }

        db.queue_work([this, accepts_beve, id, rule = std::move(*rule), response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::work txn{conn};

            std::vector<unsigned int> filter_resources{rule.filters.resources.values.begin(), rule.filters.resources.values.end()};
            std::vector<std::string> filter_scopes{rule.filters.scopes.values.begin(), rule.filters.scopes.values.end()};
            std::vector<common::log_severity> filter_severities{rule.filters.severities.values.begin(), rule.filters.severities.values.end()};
            std::vector<std::string> filter_attributes{rule.filters.attributes.values.begin(), rule.filters.attributes.values.end()};
            std::string filter_attribute_values = glz::write_json(rule.filters.attribute_values.values).value_or("null");

            try {
                pqxx::result cleanup_rule;
                if constexpr (update) {
                    cleanup_rule = txn.exec(pqxx::prepped{"update_cleanup_rule"},
                        pqxx::params{
                            rule.name, rule.description, rule.enabled, rule.execution_interval.count(),
                            rule.filter_minimum_age.count(),
                            filter_resources, rule.filters.resources.type,
                            filter_scopes, rule.filters.scopes.type,
                            filter_severities, rule.filters.severities.type,
                            filter_attributes, rule.filters.attributes.type,
                            filter_attribute_values, rule.filters.attribute_values.type,
                            rule.action, rule.action_options.and_then([](const glz::generic& v) -> std::optional<std::string> {
                                if(auto r = glz::write_json(v)) {
                                    return r.value();
                                } else {
                                    return std::nullopt;
                                }
                            }),
                            id
                        }
                    );
                } else {
                    cleanup_rule = txn.exec(pqxx::prepped{"insert_cleanup_rule"},
                        pqxx::params{
                            rule.name, rule.description, rule.enabled, rule.execution_interval.count(),
                            rule.filter_minimum_age.count(),
                            filter_resources, rule.filters.resources.type,
                            filter_scopes, rule.filters.scopes.type,
                            filter_severities, rule.filters.severities.type,
                            filter_attributes, rule.filters.attributes.type,
                            filter_attribute_values, rule.filters.attribute_values.type,
                            rule.action, rule.action_options.and_then([](const glz::generic& v) -> std::optional<std::string> {
                                if(auto r = glz::write_json(v)) {
                                    return r.value();
                                } else {
                                    return std::nullopt;
                                }
                            }),
                        }
                    );
                }
                txn.commit();

                if(cleanup_rule.empty()) {
                    response.send(Pistache::Http::Code::Internal_Server_Error, "Failed to insert/update cleanup rule");
                    return;
                }

                auto row = cleanup_rule[0];
                rule.id = row["id"].as<unsigned int>();
                rule.created_at = std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["created_at_s"].as<double>()})};
                rule.updated_at = std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["updated_at_s"].as<double>()})};
                if constexpr (update) {
                    rule.last_execution = row["last_execution_s"].as<std::optional<double>>().transform([](const auto& d) {
                        return std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::duration<double>{d})};
                    });
                } else {
                    rule.last_execution = std::nullopt;
                }

                send_response(response, accepts_beve, rule);
            } catch(const pqxx::unique_violation& e) {
                response.send(Pistache::Http::Code::Conflict, std::format("Cleanup rule with name \"{}\" already exists", rule.name));
                return;
            } catch(const std::exception& e) {
                response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
                return;
            }
        });
        return Pistache::Rest::Route::Result::Ok;
    };
    router.put("/api/v1/settings/cleanup_rules", [create_or_update_cleanup_rule](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        return create_or_update_cleanup_rule.template operator()<false>(request, std::move(response));
    });
    router.patch("/api/v1/settings/cleanup_rules/:id", [create_or_update_cleanup_rule](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        return create_or_update_cleanup_rule.template operator()<true>(request, std::move(response));
    });
    router.del("/api/v1/settings/cleanup_rules/:id", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        auto id = request.param(":id").as<unsigned int>();
        db.queue_work([this, id, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::work txn{conn};
            try {
                auto result = txn.exec(pqxx::prepped{"delete_cleanup_rule"}, id);
                if(result.affected_rows() == 0) {
                    response.send(Pistache::Http::Code::Not_Found, std::format("Cleanup rule with id {} not found", id));
                    return;
                }
                txn.commit();
                response.send(Pistache::Http::Code::No_Content);
            } catch(const std::exception& e) {
                response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
            }
        });
        return Pistache::Rest::Route::Result::Ok;
    });


    router.get("/api/v1/settings/alert_rules", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        bool accepts_beve = accepts(request, mime::application_beve);
        db.queue_work([this, accepts_beve, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};

            common::alert_rules_response res{db.get_alert_rules(txn)};
            send_response(response, accepts_beve, res);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    auto create_or_update_alert_rule = [this]<bool update>(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        bool accepts_beve = accepts(request, mime::application_beve);

        std::expected<common::alert_rule, glz::error_ctx> rule;
        if(isContentType(request, mime::application_json)) {
            rule = glz::read<common::json_opts, common::alert_rule>(request.body());
        } else if(isContentType(request, mime::application_beve)) {
            rule = glz::read<common::beve_opts, common::alert_rule>(request.body());
        } else {
            response.send(Pistache::Http::Code::Unsupported_Media_Type, "Unsupported media type");
            return Pistache::Rest::Route::Result::Ok;
        }
        if(!rule) {
            response.send(Pistache::Http::Code::Bad_Request, std::format("Failed to parse request body: {}", glz::format_error(rule.error(), request.body())));
            return Pistache::Rest::Route::Result::Ok;
        }

        if(auto res = check_alert_rule(*rule); !res) {
            response.send(Pistache::Http::Code::Bad_Request, std::format("Invalid request body: {}", res.error()));
            return Pistache::Rest::Route::Result::Ok;
        }

        unsigned int id = 0;
        if constexpr (update) {
            id = request.param(":id").as<unsigned int>();
            if(id != rule->id) {
                response.send(Pistache::Http::Code::Bad_Request, "ID in URL and body do not match");
                return Pistache::Rest::Route::Result::Ok;
            }
        }

        db.queue_work([this, accepts_beve, id, rule = std::move(*rule), response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::work txn{conn};

            std::vector<unsigned int> filter_resources{rule.filters.resources.values.begin(), rule.filters.resources.values.end()};
            std::vector<std::string> filter_scopes{rule.filters.scopes.values.begin(), rule.filters.scopes.values.end()};
            std::vector<common::log_severity> filter_severities{rule.filters.severities.values.begin(), rule.filters.severities.values.end()};
            std::vector<std::string> filter_attributes{rule.filters.attributes.values.begin(), rule.filters.attributes.values.end()};
            std::string filter_attribute_values = glz::write_json(rule.filters.attribute_values.values).value_or("null");

            try {
                pqxx::result alert_rule;
                if constexpr (update) {
                    alert_rule = txn.exec(pqxx::prepped{"update_alert_rule"},
                        pqxx::params{
                            rule.name, rule.description, rule.enabled,
                            rule.notification_provider, glz::write_json(rule.notification_options).value_or("{}"),
                            filter_resources, rule.filters.resources.type,
                            filter_scopes, rule.filters.scopes.type,
                            filter_severities, rule.filters.severities.type,
                            filter_attributes, rule.filters.attributes.type,
                            filter_attribute_values, rule.filters.attribute_values.type,
                            id
                        }
                    );
                } else {
                    alert_rule = txn.exec(pqxx::prepped{"insert_alert_rule"},
                        pqxx::params{
                            rule.name, rule.description, rule.enabled,
                            rule.notification_provider, glz::write_json(rule.notification_options).value_or("{}"),
                            filter_resources, rule.filters.resources.type,
                            filter_scopes, rule.filters.scopes.type,
                            filter_severities, rule.filters.severities.type,
                            filter_attributes, rule.filters.attributes.type,
                            filter_attribute_values, rule.filters.attribute_values.type
                        }
                    );
                }
                txn.notify("alert_rules");
                txn.commit();

                if(alert_rule.empty()) {
                    response.send(Pistache::Http::Code::Internal_Server_Error, "Failed to insert/update alert rule");
                    return;
                }

                auto row = alert_rule[0];
                rule.id = row["id"].as<unsigned int>();
                rule.created_at = std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["created_at_s"].as<double>()})};
                rule.updated_at = std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["updated_at_s"].as<double>()})};
                if constexpr (update) {
                    rule.last_alert = row["last_alert_s"].as<std::optional<double>>().transform([](const auto& d) {
                        return std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::duration<double>{d})};
                    });
                    rule.last_alert_successful = row["last_alert_successful"].as<std::optional<bool>>();
                    rule.last_alert_message = row["last_alert_message"].as<std::optional<std::string>>();
                } else {
                    rule.last_alert = std::nullopt;
                    rule.last_alert_successful = std::nullopt;
                    rule.last_alert_message = std::nullopt;
                }

                send_response(response, accepts_beve, rule);
            } catch(const pqxx::unique_violation& e) {
                response.send(Pistache::Http::Code::Conflict, std::format("Alert rule with name \"{}\" already exists", rule.name));
                return;
            } catch(const std::exception& e) {
                response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
                return;
            }
        });
        return Pistache::Rest::Route::Result::Ok;
    };
    router.put("/api/v1/settings/alert_rules", [create_or_update_alert_rule](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        return create_or_update_alert_rule.template operator()<false>(request, std::move(response));
    });
    router.patch("/api/v1/settings/alert_rules/:id", [create_or_update_alert_rule](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        return create_or_update_alert_rule.template operator()<true>(request, std::move(response));
    });
    router.del("/api/v1/settings/alert_rules/:id", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        auto id = request.param(":id").as<unsigned int>();
        db.queue_work([this, id, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::work txn{conn};
            try {
                auto result = txn.exec(pqxx::prepped{"delete_alert_rule"}, id);
                if(result.affected_rows() == 0) {
                    response.send(Pistache::Http::Code::Not_Found, std::format("Alert rule with id {} not found", id));
                    return;
                }
                txn.notify("alert_rules");
                txn.commit();
                response.send(Pistache::Http::Code::No_Content);
            } catch(const std::exception& e) {
                response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
            }
        });
        return Pistache::Rest::Route::Result::Ok;
    });
}

}
