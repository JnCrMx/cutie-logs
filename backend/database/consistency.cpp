module;
#include <chrono>
#include <optional>
#include <string>
#include <thread>

module backend.database;
import spdlog;

namespace backend::database {

constexpr bool fast_integrity_check = true;

void Database::ensure_consistency() {
    ensure_consistency(connections.front());
}

void Database::ensure_consistency(pqxx::connection& conn) {
    pqxx::work txn{conn};
    ensure_consistency(txn);
    txn.commit();
}

void Database::ensure_consistency(pqxx::transaction_base& txn) {
    logger->info("Ensuring database consistency, this may take a while...");

    auto attributes_result = txn.exec("SELECT key, count(*) FROM(SELECT jsonb_object_keys(attributes) as key FROM logs) AS t GROUP BY key;");
    for(auto [attribute, count] : attributes_result.iter<std::string, unsigned int>()) {
        unsigned int count_null{}, count_number{}, count_string{}, count_boolean{}, count_array{}, count_object{};

        if constexpr (!fast_integrity_check) {
            auto type_result = txn.exec("SELECT jsonb_typeof(attributes->$1) as type, COUNT(*) as count FROM logs GROUP BY type;", pqxx::params{attribute});
            for(auto [type, count] : type_result.iter<std::optional<std::string>, unsigned int>()) {
                if(!type) {
                    count_null = count;
                } else if(type == "number") {
                    count_number = count;
                } else if(type == "string") {
                    count_string = count;
                } else if(type == "boolean") {
                    count_boolean = count;
                } else if(type == "array") {
                    count_array = count;
                } else if(type == "object") {
                    count_object = count;
                }
            }
        }

        bool completed = false;
        do {
            try {
                auto res = txn.exec(R"(INSERT INTO log_attributes
                    (attribute, count, count_null, count_number, count_string, count_boolean, count_array, count_object)
                    VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
                    ON CONFLICT (attribute) DO UPDATE SET
                        count = EXCLUDED.count,
                        count_null = EXCLUDED.count_null,
                        count_number = EXCLUDED.count_number,
                        count_string = EXCLUDED.count_string,
                        count_boolean = EXCLUDED.count_boolean,
                        count_array = EXCLUDED.count_array,
                        count_object = EXCLUDED.count_object;)",
                    {attribute, count, count_null, count_number, count_string, count_boolean, count_array, count_object});
                if(res.affected_rows() != 1) {
                    logger -> warn("Update of attribute {} returned {} rows, this is weird", attribute, res.affected_rows());
                }
                completed = true;
            } catch (const pqxx::deadlock_detected& e) {
                logger->warn("Deadlock detected while inserting attribute {}, retrying...", attribute);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (const std::exception& e) {
                logger->error("Internal error while inserting attribute {}: {}", attribute, e.what());
                throw;
            }
        } while(!completed);
        logger->debug("Attribute {}: null={}, number={}, string={}, boolean={}, array={}, object={}", attribute, count_null, count_number, count_string, count_boolean, count_array, count_object);
    }
    logger->info("Database consistency check complete");
}

}
