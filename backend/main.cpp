#include <algorithm>
#include <concepts>
#include <iostream>
#include <map>
#include <memory>
#include <optional>

import pistache;
import pqxx;
import argparse;
import spdlog;

import common;
import backend.database;
import backend.jobs;
import backend.opentelemetry;
import backend.web;
import backend.notifications;
import backend.self_sink;

template<typename T>
std::optional<T> get_env(std::string arg) {
    // remove leading dashes
    if(arg.starts_with("--")) {
        arg = arg.substr(2);
    } else if(arg.starts_with("-")) {
        arg = arg.substr(1);
    }
    // replace - with _
    std::replace(arg.begin(), arg.end(), '-', '_');
    // convert to uppercase
    std::transform(arg.begin(), arg.end(), arg.begin(), ::toupper);
    arg = "CUTIE_LOGS_" + arg;
    if(auto env_value = std::getenv(arg.c_str())) {
        if constexpr (std::is_same_v<T, bool>) {
            return std::string_view{env_value} == "1";
        } else if constexpr (std::convertible_to<std::string, T>) {
            return T{std::string{env_value}};
        } else {
            static_assert(false, "Unsupported type for env_get");
        }
    }
    return {};
}

template<typename T = std::string>
T env_get(argparse::ArgumentParser& parser, std::string_view arg) {
    if(parser.is_used(arg)) {
        return parser.get<T>(arg);
    }
    if(auto env_value = get_env<T>(std::string{arg})) {
        return *env_value;
    }
    return parser.get<T>(arg);
}

template<typename T = std::string>
std::optional<T> env_present(argparse::ArgumentParser& parser, std::string_view arg) {
    if(parser.is_used(arg)) {
        return parser.get<T>(arg);
    }
    if(auto env_value = get_env<T>(std::string{arg})) {
        return *env_value;
    }
    return parser.present<T>(arg);
}

int main(int argc, char** argv) {
    spdlog::cfg::load_env_levels();

    argparse::ArgumentParser program(std::string{common::project_name}, std::string{common::project_version});
    program.add_argument("--otel-address").default_value("0.0.0.0:4318")
        .help("Address to listen for OpenTelemetry requests on (env: CUTIE_LOGS_OTEL_ADDRESS)")
        .nargs(1).metavar("ADDRESS");
    program.add_argument("--web-address").default_value("127.0.0.1:8080")
        .help("Address to serve web interface on (env: CUTIE_LOGS_WEB_ADDRESS)")
        .nargs(1).metavar("ADDRESS");
    program.add_argument("--web-dev-path")
        .help("Path to serve static files from in development mode (env: CUTIE_LOGS_WEB_DEV_PATH)")
        .nargs(1).metavar("PATH");
    program.add_argument("--skip-database-consistency").default_value(false)
        .help("Skip database consistency check (env: CUTIE_LOGS_SKIP_DATABASE_CONSISTENCY)")
        .implicit_value(true);
    program.add_argument("--disable-web").default_value(false)
        .help("Disable the web interface (env: CUTIE_LOGS_DISABLE_WEB)")
        .implicit_value(true);
    program.add_argument("--geoip-country-url")
        .help("URL to download GeoLite2-Country database from (env: CUTIE_LOGS_GEOIP_COUNTRY_URL)")
        .nargs(1).metavar("URL");
    program.add_argument("--geoip-asn-url")
        .help("URL to download GeoLite2-ASN database from (env: CUTIE_LOGS_GEOIP_ASN_URL)")
        .nargs(1).metavar("URL");
    program.add_argument("--geoip-city-url")
        .help("URL to download GeoLite2-City database from (env: CUTIE_LOGS_GEOIP_CITY_URL)")
        .nargs(1).metavar("URL");
    program.add_argument("--self-ingest").default_value(false)
        .help("Ingest internal instance logs back into the local database (env: CUTIE_LOGS_SELF_INGEST)")
        .implicit_value(true);
    auto& db_arg = program.add_argument("--database", "--database-url")
        .help("Database connection string (env: CUTIE_LOGS_DATABASE_URL)")
        .nargs(1).metavar("CONNECTION_STRING");
    if(!std::getenv("CUTIE_LOGS_DATABASE_URL")) { db_arg.required(); }

    try {
        program.parse_args(argc, argv);
    } catch(const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 2;
    }

    using namespace backend;
    spdlog::info("Starting {} version {}", common::project_name, common::project_version);

    database::Database db(env_get<std::string>(program, "--database-url"));
    db.run_migrations();
    if(!env_get<bool>(program, "--skip-database-consistency")) {
        db.ensure_consistency();
    } else {
        spdlog::warn("Skipping database consistency check");
    }
    db.start_workers();

    if(env_get<bool>(program, "--self-ingest")) {
        auto logger = spdlog::default_logger();
        auto db_sink = std::make_shared<backend::self_sink_mt>(db);
        logger->sinks().push_back(db_sink); // this is not thread-safe, but should be okay I hope
        logger->info("Self-ingestion enabled");
    }

    static common::shared_settings settings{};
    if(auto country_url = env_present(program, "--geoip-country-url")) {
        settings.geoip.country_url = *country_url;
    }
    if(auto asn_url = env_present(program, "--geoip-asn-url")) {
        settings.geoip.asn_url = *asn_url;
    }
    if(auto city_url = env_present(program, "--geoip-city-url")) {
        settings.geoip.city_url = *city_url;
    }
    for(const auto& [name, provider] : notifications::registry::instance().get_providers()) {
        settings.notification_providers[name] = provider.second;
    }

    jobs::Jobs job_runner(db);
    job_runner.start();

    web::Server web_server(db, settings, Pistache::Address(env_get(program, "--web-address")));
    if(auto path = env_present(program, "--web-dev-path")) {
        spdlog::info("Serving static frontend files from {} instead of embedded files", *path);
        web_server.set_static_dev_path(*path);
    }
    if(!env_get<bool>(program, "--disable-web")) {
        web_server.serve_threaded();
    }

    opentelemetry::Server opentelemetry_server(db, Pistache::Address(env_get(program, "--otel-address")));
    opentelemetry_server.serve();

    return 0;
}
