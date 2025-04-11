module;
#include <array>
#include <stdexcept>
#include <string_view>

module backend.database;

namespace backend::database {

#include "migrations.inc"

void Database::run_migrations() {
    pqxx::work txn(connections.front());
    txn.exec(R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            applied TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            sql TEXT NOT NULL
        )
    )");

    logger->info("Running migrations");
    for(unsigned int i = 0; i < migrations.size(); i++) {
        auto r = txn.exec("SELECT sql FROM schema_migrations WHERE version = $1", i);
        if(r.empty()) {
            logger->debug("Applying migration {:04}", i);
            txn.exec(migrations[i]);
            txn.exec("INSERT INTO schema_migrations (version, sql) VALUES ($1, $2)", {i, migrations[i]});
        } else if(r.one_row().at("sql").as<std::string>() != migrations[i]) {
            logger->error("Migration {:04} has been modified since it was applied", i);
            throw std::runtime_error("Migration has been modified since it was applied");
        } else {
            logger->trace("Migration {:04} already applied", i);
        }
    }
    txn.commit(); // if one of the migrations fails, we probably want to rollback all of them
    logger->info("Migrations complete");
}

}
