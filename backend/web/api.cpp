module;
#include <chrono>
#include <expected>
#include <format>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    return std::find_if(accept.begin(), accept.end(), [&type](const auto& mime) {
        return mime_equals(mime, type);
    }) != accept.end();
}

template<typename T>
void send_response(Pistache::Http::ResponseWriter& response, bool beve, const T& data) {
    auto res_data = beve ? *glz::write_beve(data) : *glz::write_json(data);
    response.send(Pistache::Http::Code::Ok, res_data.data(), res_data.size(),
        beve ? mime::application_beve : mime::application_json);
}

template<typename T>
T parse_array(std::string_view sv, pqxx::connection& conn) {
    pqxx::array<std::ranges::range_value_t<T>> arr{sv, conn};
    return T{arr.cbegin(), arr.cend()};
}

std::string url_decode(const std::string& str) {
    std::string ret;
    for(size_t i=0; i<str.size(); i++) {
        if(str[i] == '%') {
            ret.push_back(std::stoi(str.substr(i+1, 2), nullptr, 16));
            i += 2;
        } else {
            ret.push_back(str[i]);
        }
    }
    return ret;
}

std::string build_scope_filter(const std::string& scopes) {
    std::string filter = "scope IN (";
    for(const auto& scope : scopes | std::views::split(',')) {
        filter += "'";
        std::string_view sv{scope};
        if(sv != "<empty>") {
            filter += sv;
        }
        filter += "',";
    }
    filter.pop_back();
    filter += ")";
    return filter;
}
std::string build_resource_filter(const std::string& resources) {
    std::string filter = "resource IN (";
    for(const auto& resource : resources | std::views::split(',')) {
        filter += std::to_string(std::stoi(std::string{std::string_view{resource}}));
        filter += ",";
    }
    filter.pop_back();
    filter += ")";
    return filter;
}

std::string build_query(pqxx::transaction_base& txn, const std::string& attributes, const std::string& scopes, const std::string& resources, const std::string& filter, const std::string& limit, const std::string& offset) {
    std::string query = "SELECT resource, timestamp, extract(epoch from timestamp) as unix_time, scope, severity, body";
    if(attributes == "*") {
        query += ", attributes";
    } else {
        for(const auto& attr : attributes | std::views::split(',')) {
            query += ", attributes->'";
            query += txn.esc(std::string_view{attr});
            query += "'";
        }
    }
    query += " FROM logs";
    query += " WHERE TRUE";
    if(!scopes.empty()) {
        query += " AND " + build_scope_filter(scopes);
    }
    if(!resources.empty()) {
        query += " AND " + build_resource_filter(resources);
    }
    query += " ORDER BY timestamp DESC";
    query += " LIMIT " + std::to_string(std::stoi(limit));
    query += " OFFSET " + std::to_string(std::stoi(offset));
    return query;
}

std::vector<common::log_entry> get_logs(pqxx::transaction_base& txn, const std::string& attributes, const std::string& scopes, const std::string& resources, const std::string& filter, const std::string& limit, const std::string& offset) {
    std::string query = build_query(txn, attributes, scopes, resources, filter, limit, offset);

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
        l.body = row["body"].as<std::optional<glz::json_t>>().value_or(glz::json_t::null_t{});
        unsigned int i=row.column_number("body")+1;
        if(attributes == "*") {
            l.attributes = row["attributes"].as<glz::json_t>();
        } else {
            for(const auto& attr : attributes | std::views::split(',')) {
                auto sv = std::string_view{attr};
                auto json = row[i].as<std::optional<glz::json_t>>();
                if(json) {
                    l.attributes[sv] = std::move(*json);
                }
                i++;
            }
        }
    }
    return logs;
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
        auto filter = request.query().get("filter").transform(url_decode).value_or("");
        auto attributes = request.query().get("attributes").transform(url_decode).value_or("");
        auto scopes = request.query().get("scopes").transform(url_decode).value_or("");
        auto resources = request.query().get("resources").transform(url_decode).value_or("");
        auto limit = request.query().get("limit").transform(url_decode).value_or("100");
        auto offset = request.query().get("offset").transform(url_decode).value_or("0");
        db.queue_work([this, accepts_beve, response = std::move(response), filter, attributes, scopes, resources, limit, offset](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            try {
                common::logs_response res;
                res.logs = get_logs(txn, attributes, scopes, resources, filter, limit, offset);

                send_response(response, accepts_beve, res);
            } catch(const pqxx::sql_error& e) {
                response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
            }
            malloc_trim(1024*1024);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/logs/stencil", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        auto filter = request.query().get("filter").transform(url_decode).value_or("");
        auto scopes = request.query().get("scopes").transform(url_decode).value_or("");
        auto resources = request.query().get("resources").transform(url_decode).value_or("");
        auto limit = request.query().get("limit").transform(url_decode).value_or("100");
        auto offset = request.query().get("offset").transform(url_decode).value_or("0");
        auto stencil = request.query().get("stencil").transform(url_decode).value_or("");
        db.queue_work([this, response = std::move(response), scopes, resources, filter, limit, offset, stencil](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            try {
                auto stream = response.stream(Pistache::Http::Code::Ok);
                std::string query = "SELECT resource, extract(epoch from timestamp) as unix_time, scope, severity, attributes, body FROM logs WHERE TRUE";
                if(!scopes.empty()) {
                    query += " AND " + build_scope_filter(scopes);
                }
                if(!resources.empty()) {
                    query += " AND " + build_resource_filter(resources);
                }
                query += " ORDER BY timestamp DESC";
                query += " LIMIT " + std::to_string(std::stoi(limit));
                query += " OFFSET " + std::to_string(std::stoi(offset));

                for(auto [resource, timestamp, scope, severity, attributes, body] :
                    txn.stream<unsigned int, double, std::string, common::log_severity, glz::json_t, glz::json_t>(query))
                {
                    common::log_entry log{resource, timestamp, scope, severity, attributes, body};
                    std::string line = *common::stencil(stencil, log)
                        .or_else([](auto err) -> std::expected<std::string, std::string> { return std::format("Stencil invalid: \"{}\"", err); })
                        +"\n";
                    stream.write(line.data(), line.size());
                }
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
            common::logs_scopes_response res;
            for(const auto& row : result) {
                res.scopes[row["scope"].as<std::string>()] = row["count"].as<int>();
            }
            res.total_logs = txn.exec(pqxx::prepped{"get_count"}).one_field().as<unsigned int>();

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
                r.attributes = row["attributes"].as<glz::json_t>();
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
            auto result = txn.exec(pqxx::prepped{"get_cleanup_rules"});
            common::cleanup_rules_response res;

            for(const auto& row : result) {
                unsigned int id = row["id"].as<unsigned int>();
                common::cleanup_rule& rule = res.rules[id];

                rule.id = id;
                rule.name = row["name"].as<std::string>();
                rule.description = row["description"].as<std::string>();
                rule.enabled = row["enabled"].as<bool>();
                rule.execution_interval = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["execution_interval_s"].as<double>()});

                rule.filter_minimum_age = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["filter_minimum_age_s"].as<double>()});
                rule.filter_resources.values = parse_array<std::unordered_set<unsigned int>>(row["filter_resources"].as<std::string>(), conn);
                rule.filter_resources.type = row["filter_resources_type"].as<common::filter_type>();
                rule.filter_scopes.values = parse_array<std::unordered_set<std::string>>(row["filter_scopes"].as<std::string>(), conn);
                rule.filter_scopes.type = row["filter_scopes_type"].as<common::filter_type>();
                rule.filter_severities.values = parse_array<std::unordered_set<common::log_severity>>(row["filter_severities"].as<std::string>(), conn);
                rule.filter_severities.type = row["filter_severities_type"].as<common::filter_type>();
                rule.filter_attributes.include = row["filter_attributes_include"].as<std::optional<std::string>>().transform([&conn](const auto& str) {
                    return parse_array<std::unordered_set<std::string>>(str, conn);
                });
                rule.filter_attributes.exclude = row["filter_attributes_exclude"].as<std::optional<std::string>>().transform([&conn](const auto& str) {
                    return parse_array<std::unordered_set<std::string>>(str, conn);
                });
                rule.filter_attribute_values.include = row["filter_attribute_values_include"].as<std::optional<glz::json_t>>();
                rule.filter_attribute_values.exclude = row["filter_attribute_values_exclude"].as<std::optional<glz::json_t>>();

                rule.created_at = std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["created_at_s"].as<double>()})};
                rule.updated_at = std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::duration<double>{row["updated_at_s"].as<double>()})};
                rule.last_execution = row["last_execution_s"].as<std::optional<double>>().transform([](const auto& d) {
                    return std::chrono::sys_seconds{std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::duration<double>{d})};
                });
            }
            send_response(response, accepts_beve, res);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
}

}
