module;
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <glaze/util/fast_float.hpp>

module backend.web;

import glaze;
import pqxx;

import common;

namespace backend::web {

namespace mime {
static const auto application_json = Pistache::Http::Mime::MediaType{Pistache::Http::Mime::Type::Application, Pistache::Http::Mime::Subtype::Json};
static const auto application_octet = Pistache::Http::Mime::MediaType{Pistache::Http::Mime::Type::Application, Pistache::Http::Mime::Subtype::OctetStream};
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
        auto attrs = request.query().get("attrs").value_or("");
        auto limit = request.query().get("limit").value_or("100");
        auto offset = request.query().get("offset").value_or("0");
        db.queue_work([this, response = std::move(response), filter, attrs, limit, offset](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            std::string query = "SELECT resource, timestamp, extract(epoch from timestamp) as unix_time, scope, severity, body";
            for(const auto& attr : attrs | std::views::split(',')) {
                query += ", attributes->'";
                query += txn.esc(std::string_view{attr});
                query += "'";
            }
            query += " FROM logs";
            query += " ORDER BY timestamp DESC";
            query += " LIMIT " + std::to_string(std::stoi(limit));
            query += " OFFSET " + std::to_string(std::stoi(offset));

            logger->trace("Executing query: {}", query);
            try {
                auto result = txn.exec(query);
                common::logs_response res;
                res.logs.reserve(result.size());
                for(const auto& row : result) {
                    auto& l = res.logs.emplace_back(
                        row["resource"].as<unsigned int>(),
                        row["unix_time"].as<double>(),
                        row["scope"].as<std::string>(),
                        row["severity"].as<common::log_severity>()
                    );
                    l.body = glz::json_t::object_t{};
                    unsigned int i=row.column_number("body")+1;
                    for(const auto& attr : attrs | std::views::split(',')) {
                        auto sv = std::string_view{attr};
                        l.attributes[sv] = *glz::read_json<glz::json_t>(row[i].as<std::optional<std::string>>().value_or("null"));
                    }
                }
                auto beve = *glz::write_beve(res);
                response.send(Pistache::Http::Code::Ok, beve.data(), beve.size(), mime::application_octet);
            } catch(const pqxx::sql_error& e) {
                response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
            }
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
