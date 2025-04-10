#include <concepts>
#include <iostream>
#include <optional>

import pistache;
import pqxx;
import argparse;
import spdlog;

import common;
import backend.database;
import backend.opentelemetry;
import backend.web;

template<typename T = const char*>
argparse::Argument& env_default(argparse::Argument& arg, std::string env_var) {
    if(auto env = std::getenv(env_var.c_str())) {
        if constexpr (std::is_same_v<T, bool>) {
            return arg.default_value(std::string_view{env} == "1");
        } else if constexpr (std::convertible_to<T, std::string>) {
            return arg.default_value(std::string{env});
        } else {
            static_assert(false, "Unsupported type for env_default");
        }
    }
    return arg;
}
template<typename T>
argparse::Argument& env_default(argparse::Argument& arg, std::string env_var, T default_value) {
    return env_default<T>(arg.default_value(default_value), env_var);
}

int main(int argc, char** argv) {
    spdlog::cfg::load_env_levels();

    argparse::ArgumentParser program(std::string{common::project_name}, std::string{common::project_version});
    env_default(program.add_argument("--otel-address"), "CUTIE_LOGS_OTEL_ADDRESS", "0.0.0.0:4318")
        .help("Address to listen for OpenTelemetry requests on (env: CUTIE_LOGS_OTEL_ADDRESS)")
        .nargs(1).metavar("ADDRESS");
    env_default(program.add_argument("--web-address"), "CUTIE_LOGS_WEB_ADDRESS", "127.0.0.1:8080")
        .help("Address to serve web interface on (env: CUTIE_LOGS_WEB_ADDRESS)")
        .nargs(1).metavar("ADDRESS");
    env_default(program.add_argument("--web-dev-path"), "CUTIE_LOGS_WEB_DEV_PATH")
        .help("Path to serve static files from in development mode (env: CUTIE_LOGS_WEB_DEV_PATH)")
        .nargs(1).metavar("PATH");
    env_default(program.add_argument("--skip-database-consistency"), "CUTIE_LOGS_SKIP_DATABASE_CONSISTENCY", false)
        .help("Skip database consistency check (env: CUTIE_LOGS_SKIP_DATABASE_CONSISTENCY)")
        .implicit_value(true);
    env_default(program.add_argument("--disable-web"), "CUTIE_LOGS_DISABLE_WEB", false)
        .help("Disable the web interface (env: CUTIE_LOGS_DISABLE_WEB)")
        .implicit_value(true);
    auto& arg_db_url = env_default(program.add_argument("--database", "--database-url"), "CUTIE_LOGS_DATABASE_URL")
        .help("Database connection string (env: CUTIE_LOGS_DATABASE_URL)")
        .required()
        .nargs(1).metavar("CONNECTION_STRING");

    try {
        program.parse_args(argc, argv);
    } catch(const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 2;
    }

    using namespace backend;
    spdlog::info("Starting {} version {}", common::project_name, common::project_version);

    database::Database db(program.get<std::string>("--database"));
    db.run_migrations();
    if(!program.get<bool>("--skip-database-consistency")) {
        db.ensure_consistency();
    } else {
        spdlog::warn("Skipping database consistency check");
    }
    db.start_workers();

    web::Server web_server(db, Pistache::Address(program.get<std::string>("--web-address")));
    if(auto path = program.present("--web-dev-path")) {
        spdlog::info("Serving static frontend files from {} instead of embedded files", *path);
        web_server.set_static_dev_path(*path);
    }
    if(!program.get<bool>("--disable-web")) {
        web_server.serve_threaded();
    }

    opentelemetry::Server opentelemetry_server(db, Pistache::Address(program.get<std::string>("--otel-address")));
    opentelemetry_server.serve();

    return 0;
}
