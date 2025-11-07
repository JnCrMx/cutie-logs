module;
#include <chrono>
#include <expected>
#include <future>
#include <map>
#include <utility>

module backend.jobs;
import spdlog;
import pqxx;
import glaze;

import common;

namespace backend::jobs {

std::string craft_cleanup_job_filter_sql(const common::cleanup_rule& rule, pqxx::connection& conn) {
    std::string sql = "timestamp < NOW() - '" + std::to_string(rule.filter_minimum_age.count()) + " seconds'::interval";
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

std::expected<int, std::string> execute_drop_cleanup_job(const common::cleanup_rule& rule, pqxx::connection& conn, spdlog::logger& logger) {
    std::string sql = "DELETE FROM logs WHERE " + craft_cleanup_job_filter_sql(rule, conn);
    if(sql.ends_with(" WHERE ")) {
        logger.warn("Filter crafting failed for drop cleanup job {}:{}: {}", rule.id, rule.name, sql);
        return std::unexpected("Failed to craft SQL for drop cleanup job");
    }
    logger.debug("Executing drop cleanup job {}:{} with SQL: {}", rule.id, rule.name, sql);

    try {
        pqxx::work txn(conn);
        std::size_t affected_rows = txn.exec(sql).affected_rows();
        txn.exec(pqxx::prepped{"complete_cleanup_rule"}, pqxx::params{rule.id});
        txn.commit();

        return affected_rows;
    } catch(const std::exception& e) {
        return std::unexpected(std::string("Error executing drop cleanup job: ") + e.what());
    }
}

std::expected<int, std::string> execute_transform_cleanup_job(const common::cleanup_rule& rule, pqxx::connection& conn, spdlog::logger& logger) {
    if(!rule.action_options) {
        return std::unexpected("No action options provided for transform cleanup job");
    }
    constexpr auto json_options = glz::opts{ // extra opts for safety
        .format = glz::JSON,
        .error_on_unknown_keys = true,
        .error_on_missing_keys = true
    };
    auto actions_expected = glz::read<json_options, std::vector<common::transform_action>>(*rule.action_options);
    if(!actions_expected) {
        return std::unexpected(std::string("Error parsing action options for transform cleanup job: ") + glz::format_error(actions_expected.error()));
    }
    const auto& actions = *actions_expected;
    if(actions.empty()) {
        return std::unexpected("No actions provided for transform cleanup job");
    }

    std::string sql = "SELECT * FROM logs WHERE " + craft_cleanup_job_filter_sql(rule, conn);
    if(sql.ends_with(" WHERE ")) {
        logger.warn("Filter crafting failed for transform cleanup job {}:{}: {}", rule.id, rule.name, sql);
        return std::unexpected("Failed to craft SQL for transform cleanup job");
    }
    logger.debug("Executing transform cleanup job {}:{} with SQL: {}", rule.id, rule.name, sql);

    try {
        pqxx::work txn(conn);

        int total_affected_rows = 0;
        for(auto [resource, timestamp, scope, severity, attributes, body] :
            txn.stream<unsigned int, double, std::string, common::log_severity, glz::generic, glz::generic>(sql))
        {
            common::log_entry log{resource, timestamp, scope, severity, attributes, body};
            auto& attrs_obj = log.attributes.get_object();

            for(const auto& action : actions) {
                switch(action.type) {
                    case common::transform_action_type::REMOVE_ATTRIBUTE:
                        attrs_obj.erase(action.attribute);
                        break;
                    case common::transform_action_type::SET_ATTRIBUTE:
                        if(!action.stencil) {
                            logger.warn("No stencil provided for transform action in cleanup job {}:{}", rule.id, rule.name);
                            continue;
                        }
                        if(auto value_expected = common::stencil(*action.stencil, log); value_expected) {
                            attrs_obj[action.attribute] = std::move(*value_expected);
                        } else {
                            logger.warn("Error applying stencil for transform action in cleanup job {}:{}: {}", rule.id, rule.name, value_expected.error());
                        }
                        break;
                    default:
                        logger.error("Unsupported transform action type {} in cleanup job {}:{}", std::to_underlying(action.type), rule.id, rule.name);
                        continue;
                }
            }
            int affected_rows = txn.exec(pqxx::prepped{"update_log_attributes"},
                pqxx::params{
                    log.resource, log.timestamp, log.scope,
                    glz::write_json(log.attributes).value_or("{}")
                }
            ).affected_rows();

            if(affected_rows > 0) {
                total_affected_rows += affected_rows;
            } else {
                logger.warn("No rows affected when updating log in transform cleanup job {}:{}", rule.id, rule.name);
            }
        }

        txn.exec(pqxx::prepped{"complete_cleanup_rule"}, pqxx::params{rule.id});
        txn.commit();

        return total_affected_rows;
    } catch(const std::exception& e) {
        return std::unexpected(std::string("Error executing transform cleanup job: ") + e.what());
    }

    return 0;
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

            std::expected<int, std::string> result{};
            switch(rule.action) {
                case common::rule_action::DROP:
                    result = execute_drop_cleanup_job(rule, conn, *logger);
                    break;
                case common::rule_action::TRANSFORM:
                    result = execute_transform_cleanup_job(rule, conn, *logger);
                    break;
                default:
                    logger->error("Unsupported cleanup action {} for job {}:{}", std::to_underlying(rule.action), rule.id, rule.name);
                    continue;
            }
            if(result) {
                logger->info("Executed cleanup job {}:{} successfully, affected rows: {}", rule.id, rule.name, *result);
                if(*result > 0) {
                    any_jobs_ran = true;
                }
            } else {
                logger->error("Error executing cleanup job {}:{}: {}", rule.id, rule.name, result.error());
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
