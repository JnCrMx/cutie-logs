module;
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

std::string build_query(pqxx::transaction_base& txn, const std::string& attributes, const std::string& filter, const std::string& limit, const std::string& offset) {
    std::string query = "SELECT resource, timestamp, extract(epoch from timestamp) as unix_time, scope, severity, body";
    for(const auto& attr : attributes | std::views::split(',')) {
        query += ", attributes->'";
        query += txn.esc(std::string_view{attr});
        query += "'";
    }
    query += " FROM logs";
    query += " ORDER BY timestamp DESC";
    query += " LIMIT " + std::to_string(std::stoi(limit));
    query += " OFFSET " + std::to_string(std::stoi(offset));
    return query;
}

std::vector<common::log_entry> get_logs(pqxx::transaction_base& txn, const std::string& attributes, const std::string& filter, const std::string& limit, const std::string& offset) {
    std::string query = build_query(txn, attributes, filter, limit, offset);

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
        for(const auto& attr : attributes | std::views::split(',')) {
            auto sv = std::string_view{attr};
            auto json = row[i].as<std::optional<glz::json_t>>();
            if(json) {
                l.attributes[sv] = std::move(*json);
            }
            i++;
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
    router.get("/api/v1/logs", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        auto filter = request.query().get("filter").value_or("");
        auto attributes = request.query().get("attributes").value_or("");
        auto limit = request.query().get("limit").value_or("100");
        auto offset = request.query().get("offset").value_or("0");
        db.queue_work([this, response = std::move(response), filter, attributes, limit, offset](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            try {
                common::logs_response res;
                res.logs = get_logs(txn, attributes, filter, limit, offset);
                auto beve = *glz::write_beve(res);
                response.send(Pistache::Http::Code::Ok, beve.data(), beve.size(), mime::application_octet);
            } catch(const pqxx::sql_error& e) {
                response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
            }
            malloc_trim(1024*1024);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/logs/stencil", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        auto filter = request.query().get("filter").value_or("");
        auto attributes = request.query().get("attributes").value_or("");
        auto limit = request.query().get("limit").value_or("100");
        auto offset = request.query().get("offset").value_or("0");
        auto stencil = request.query().get("stencil").transform(url_decode).value_or("");
        db.queue_work([this, response = std::move(response), filter, attributes, limit, offset, stencil](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            try {
                auto stream = response.stream(Pistache::Http::Code::Ok);
                std::string query = "SELECT resource, extract(epoch from timestamp) as unix_time, scope, severity, attributes, body FROM logs ORDER BY timestamp DESC";
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
        db.queue_work([this, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            auto result = txn.exec("SELECT jsonb_object_keys(attributes) as key, COUNT(*) as count FROM logs GROUP BY key;");
            common::logs_attributes_response res;
            for(const auto& row : result) {
                res.attributes[row["key"].as<std::string>()] = row["count"].as<int>();
            }
            res.total_logs = txn.exec("SELECT COUNT(*) FROM logs;").one_field().as<unsigned int>();

            auto beve = *glz::write_beve(res);
            response.send(Pistache::Http::Code::Ok, beve.data(), beve.size(), mime::application_octet);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/logs/scopes", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        db.queue_work([this, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            auto result = txn.exec("SELECT scope, COUNT(*) as count FROM logs GROUP BY scope;");
            common::logs_scopes_response res;
            for(const auto& row : result) {
                res.scopes[row["scope"].as<std::string>()] = row["count"].as<int>();
            }
            res.total_logs = txn.exec("SELECT COUNT(*) FROM logs;").one_field().as<unsigned int>();

            auto beve = *glz::write_beve(res);
            response.send(Pistache::Http::Code::Ok, beve.data(), beve.size(), mime::application_octet);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
}

}
