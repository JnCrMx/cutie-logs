module;

#include <pqxx/pqxx>

export module pqxx;

export namespace pqxx {
    using pqxx::connection;
    using pqxx::transaction_base;
    using pqxx::nontransaction;
    using pqxx::work;
    using pqxx::result;
    using pqxx::row;
    using pqxx::prepped;
    using pqxx::params;

    using pqxx::zview;
    using pqxx::string_traits;
    using pqxx::no_null;
    using pqxx::nullness;
    using pqxx::type_name;

    using pqxx::conversion_error;
    using pqxx::data_exception;
    using pqxx::deadlock_detected;
    using pqxx::failure;
    using pqxx::serialization_failure;
    using pqxx::sql_error;
    using pqxx::unexpected_rows;
    using pqxx::unique_violation;
}
