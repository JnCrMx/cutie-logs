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
}

struct log_entry {
    unsigned int resource;
    double timestamp;
    std::string scope;
    common::log_severity severity;
    glz::json_t attributes;
    glz::json_t body;
};

void Server::setupApiRoutes() {
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
                auto res = txn.exec(query);
                std::vector<log_entry> logs;
                logs.reserve(res.size());
                for(const auto& row : res) {
                    auto& l = logs.emplace_back(
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
                auto json = *glz::write_json(logs);
                response.send(Pistache::Http::Code::Ok, json, mime::application_json);
            } catch(const pqxx::sql_error& e) {
                response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
            }
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/logs/attributes", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        db.queue_work([this, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            auto res = txn.exec("SELECT jsonb_object_keys(attributes) as key, COUNT(*) as count FROM logs GROUP BY key;");
            std::unordered_map<std::string, int> attributes;
            for(const auto& row : res) {
                attributes[row["key"].as<std::string>()] = row["count"].as<int>();
            }

            auto count = txn.exec("SELECT COUNT(*) FROM logs;").one_field().as<unsigned int>();
            attributes["_"] = count;

            auto json = *glz::write_json(attributes);
            response.send(Pistache::Http::Code::Ok, json, mime::application_json);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
    router.get("/api/v1/logs/scopes", [this](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        db.queue_work([this, response = std::move(response)](pqxx::connection& conn) mutable {
            pqxx::nontransaction txn{conn};
            auto res = txn.exec("SELECT scope, COUNT(*) as count FROM logs GROUP BY scope;");
            std::unordered_map<std::string, int> scopes;
            for(const auto& row : res) {
                scopes[row["scope"].as<std::string>()] = row["count"].as<int>();
            }

            auto count = txn.exec("SELECT COUNT(*) FROM logs;").one_field().as<unsigned int>();
            scopes["_"] = count;

            auto json = *glz::write_json(scopes);
            response.send(Pistache::Http::Code::Ok, json, mime::application_json);
        });
        return Pistache::Rest::Route::Result::Ok;
    });
}

}
