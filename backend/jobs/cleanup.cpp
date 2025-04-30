module;
#include <chrono>
#include <future>
#include <unordered_map>
#include <utility>

module backend.jobs;
import spdlog;
import pqxx;

import common;

namespace backend::jobs {

std::string craft_cleanup_job_sql(const common::cleanup_rule& rule, pqxx::connection& conn) {
    std::string sql = "DELETE FROM logs WHERE ";
    sql += "timestamp < NOW() - '" + std::to_string(rule.filter_minimum_age.count()) + " seconds'::interval";
    if(!rule.filter_resources.values.empty()) {
        if(rule.filter_resources.type == common::filter_type::INCLUDE) {
            sql += " AND resource IN (";
        } else {
            sql += " AND resource NOT IN (";
        }
        for(const auto& resource : rule.filter_resources.values) {
            sql += std::to_string(resource) + ",";
        }
        sql.pop_back(); // Remove the last comma
        sql += ")";
    }
    if(!rule.filter_scopes.values.empty()) {
        if(rule.filter_scopes.type == common::filter_type::INCLUDE) {
            sql += " AND scope IN (";
        } else {
            sql += " AND scope NOT IN (";
        }
        for(const auto& scope : rule.filter_scopes.values) {
            sql += "'" + conn.esc(scope) + "',";
        }
        sql.pop_back(); // Remove the last comma
        sql += ")";
    }
    if(!rule.filter_severities.values.empty()) {
        if(rule.filter_severities.type == common::filter_type::INCLUDE) {
            sql += " AND severity IN (";
        } else {
            sql += " AND severity NOT IN (";
        }
        for(const auto& severity : rule.filter_severities.values) {
            sql += "'" + std::string{common::log_severity_names[std::to_underlying(severity)]} + "',";
        }
        sql.pop_back(); // Remove the last comma
        sql += ")";
    }
    // TODO: filter_attributes and filter_attribute_values
    return sql;
}

void Jobs::run_cleanup_jobs() {
    logger->debug("Running cleanup jobs");

    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    db.queue_work([this, promise = std::move(promise)](pqxx::connection& conn) mutable {
        std::unordered_map<unsigned int, common::cleanup_rule> jobs;
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

            if(rule.filter_resources.values.empty() && rule.filter_resources.type == common::filter_type::INCLUDE) {
                logger->warn("Skipping cleanup job {}:{} because it has a resource filter of type INCLUDE, but no resources are specified", rule.id, rule.name);
                continue;
            }
            if(rule.filter_scopes.values.empty() && rule.filter_scopes.type == common::filter_type::INCLUDE) {
                logger->warn("Skipping cleanup job {}:{} because it has a scope filter of type INCLUDE, but no scopes are specified", rule.id, rule.name);
                continue;
            }
            if(rule.filter_severities.values.empty() && rule.filter_severities.type == common::filter_type::INCLUDE) {
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
