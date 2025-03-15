module;
#include <array>
#include <string_view>

module backend.database;

namespace backend::database {

constexpr char migration_0000_init[] = {
    #embed "0000_init.sql"
};
std::array migrations = {
    std::string_view{migration_0000_init}
};

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
        auto r = txn.exec("SELECT 1 FROM schema_migrations WHERE version = $1", i);
        if(r.empty()) {
            logger->debug("Applying migration {:04}", i);
            txn.exec(migrations[i]);
            txn.exec("INSERT INTO schema_migrations (version, sql) VALUES ($1, $2)", {i, migrations[i]});
        } else {
            logger->trace("Migration {:04} already applied", i);
        }
    }
    txn.commit();
    logger->info("Migrations complete");
}

}
