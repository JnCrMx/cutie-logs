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
    using pqxx::notification;

    using pqxx::zview;
    using pqxx::string_traits;
    using pqxx::no_null;
    using pqxx::nullness;
    using pqxx::type_name;
    using pqxx::conversion_context;
    using pqxx::ctx;

    using pqxx::array;

    using pqxx::failure;
    using pqxx::broken_connection;
    using pqxx::version_mismatch;
    using pqxx::variable_set_to_null;
    using pqxx::sql_error;
    using pqxx::protocol_violation;
    using pqxx::in_doubt_error;
    using pqxx::transaction_rollback;
    using pqxx::serialization_failure;
    using pqxx::statement_completion_unknown;
    using pqxx::deadlock_detected;
    using pqxx::internal_error;
    using pqxx::usage_error;
    using pqxx::argument_error;
    using pqxx::conversion_error;
    using pqxx::unexpected_null;
    using pqxx::conversion_overrun;
    using pqxx::range_error;
    using pqxx::unexpected_rows;
    using pqxx::feature_not_supported;
    using pqxx::data_exception;
    using pqxx::integrity_constraint_violation;
    using pqxx::restrict_violation;
    using pqxx::not_null_violation;
    using pqxx::foreign_key_violation;
    using pqxx::unique_violation;
    using pqxx::check_violation;
    using pqxx::invalid_cursor_state;
    using pqxx::invalid_sql_statement_name;
    using pqxx::invalid_cursor_name;
    using pqxx::syntax_error;
    using pqxx::undefined_column;
    using pqxx::undefined_function;
    using pqxx::undefined_table;
    using pqxx::insufficient_privilege;
    using pqxx::insufficient_resources;
    using pqxx::disk_full;
    using pqxx::server_out_of_memory;
    using pqxx::too_many_connections;
    using pqxx::plpgsql_error;
    using pqxx::plpgsql_raise;
    using pqxx::plpgsql_no_data_found;
    using pqxx::plpgsql_too_many_rows;
}
