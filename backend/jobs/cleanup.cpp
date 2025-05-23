module;
#include <chrono>
#include <future>
#include <map>
#include <utility>

module backend.jobs;
import spdlog;
import pqxx;
import glaze;

import common;

namespace backend::jobs {

std::string craft_cleanup_job_sql(const common::cleanup_rule& rule, pqxx::connection& conn) {
    std::string sql = "DELETE FROM logs WHERE ";
    sql += "timestamp < NOW() - '" + std::to_string(rule.filter_minimum_age.count()) + " seconds'::interval";
    if(!rule.filters.resources.values.empty()) {
        if(rule.filters.resources.type == common::filter_type::INCLUDE) {
            sql += " AND resource IN (";
        } else {
            sql += " AND resource NOT IN (";
        }
        for(const auto& resource : rule.filters.resources.values) {
            sql += std::to_string(resource) + ",";
        }
        sql.pop_back(); // Remove the last comma
        sql += ")";
    }
    if(!rule.filters.scopes.values.empty()) {
        if(rule.filters.scopes.type == common::filter_type::INCLUDE) {
            sql += " AND scope IN (";
        } else {
            sql += " AND scope NOT IN (";
        }
        for(const auto& scope : rule.filters.scopes.values) {
            sql += "'" + conn.esc(scope) + "',";
        }
        sql.pop_back(); // Remove the last comma
        sql += ")";
    }
    if(!rule.filters.severities.values.empty()) {
        if(rule.filters.severities.type == common::filter_type::INCLUDE) {
            sql += " AND severity IN (";
        } else {
            sql += " AND severity NOT IN (";
        }
        for(const auto& severity : rule.filters.severities.values) {
            sql += "'" + std::string{common::log_severity_names[std::to_underlying(severity)]} + "',";
        }
        sql.pop_back(); // Remove the last comma
        sql += ")";
    }
    for(const auto& attr : rule.filters.attributes.values) {
        if(rule.filters.attributes.type == common::filter_type::INCLUDE) {
            sql += " AND attributes ? '" + conn.esc(attr) + "'";
        } else {
            sql += " AND NOT attributes ? '" + conn.esc(attr) + "'";
        }
    }
    if(!rule.filters.attribute_values.values.empty()) {
        std::string json = glz::write_json(rule.filters.attribute_values.values).value_or("null");
        if(rule.filters.attribute_values.type == common::filter_type::INCLUDE) {
            sql += " AND attributes @> '" + conn.esc(json) + "'::jsonb";
        } else {
            sql += " AND NOT attributes @> '" + conn.esc(json) + "'::jsonb";
        }
    }
    return sql;
}

void Jobs::run_cleanup_jobs() {
    logger->debug("Running cleanup jobs");

    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    db.queue_work([this, promise = std::move(promise)](pqxx::connection& conn) mutable {
        std::map<unsigned int, common::cleanup_rule> jobs;
        {
            pqxx::nontransaction txn(conn);
            jobs = db.get_cleanup_rules(txn);
        }
        logger->debug("Fetched {} cleanup jobs", jobs.size());

        bool any_jobs_ran = false;
        for(const auto& [_, rule] : jobs) {
            if(!rule.enabled) {
                logger->trace("Skipping cleanup job {}:{} because it is disabled", rule.id, rule.name);
                continue;
            }
            if(rule.last_execution && *rule.last_execution > std::chrono::system_clock::now() - rule.execution_interval) {
                logger->trace("Skipping cleanup job {}:{} because it was executed recently", rule.id, rule.name);
                continue;
            }

            if(rule.filters.resources.values.empty() && rule.filters.resources.type == common::filter_type::INCLUDE) {
                logger->warn("Skipping cleanup job {}:{} because it has a resource filter of type INCLUDE, but no resources are specified", rule.id, rule.name);
                continue;
            }
            if(rule.filters.scopes.values.empty() && rule.filters.scopes.type == common::filter_type::INCLUDE) {
                logger->warn("Skipping cleanup job {}:{} because it has a scope filter of type INCLUDE, but no scopes are specified", rule.id, rule.name);
                continue;
            }
            if(rule.filters.severities.values.empty() && rule.filters.severities.type == common::filter_type::INCLUDE) {
                logger->warn("Skipping cleanup job {}:{} because it has a severity filter of type INCLUDE, but no severities are specified", rule.id, rule.name);
                continue;
            }

            std::string sql = craft_cleanup_job_sql(rule, conn);
            if(sql.empty()) {
                logger->warn("Cleanup job {}:{} has no SQL to execute", rule.id, rule.name);
                continue;
            }
            logger->debug("Executing cleanup job {}:{} with SQL: {}", rule.id, rule.name, sql);

            try {
                pqxx::work txn(conn);
                std::size_t affected_rows = txn.exec(sql).affected_rows();
                txn.exec("UPDATE cleanup_rules SET last_execution = NOW() WHERE id = $1", pqxx::params{rule.id});
                txn.commit();
                logger->info("Executed cleanup job {}:{} successfully, affected rows: {}", rule.id, rule.name, affected_rows);

                if(affected_rows > 0) {
                    any_jobs_ran = true;
                }
            } catch(const std::exception& e) {
                logger->error("Error executing cleanup job {}:{}: {}", rule.id, rule.name, e.what());
            }
        }
        logger->debug("Finished executing cleanup jobs");

        if(any_jobs_ran) {
            logger->debug("Rebuilding attribute statistics");
            db.ensure_consistency(conn);
        }

        promise.set_value();
    });
    future.wait();
    logger->debug("Finished cleanup jobs");
}

}
