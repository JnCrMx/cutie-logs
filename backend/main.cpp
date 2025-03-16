import pistache;
import pqxx;
import argparse;
import spdlog;

import common;
import backend.database;
import backend.opentelemetry;
import backend.web;

#include <iostream>

int main(int argc, char** argv) {
    spdlog::cfg::load_env_levels();

    argparse::ArgumentParser program(std::string{common::project_name}, std::string{common::project_version});
    program.add_argument("--otel-address")
        .help("Address to listen for OpenTelemetry requests on")
        .default_value("0.0.0.0:4318")
        .nargs(1).metavar("ADDRESS");
    program.add_argument("--web-address")
        .help("Address to serve web interface on")
        .default_value("127.0.0.1:8080")
        .nargs(1).metavar("ADDRESS");
    program.add_argument("--web-dev-path")
        .help("Path to serve static files from in development mode")
        .nargs(1).metavar("PATH");
    program.add_argument("--disable-web")
        .help("Disable the web interface")
        .flag();
    program.add_argument("--database")
        .help("Database connection string")
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
    db.start_workers();

    web::Server web_server(db, Pistache::Address(program.get<std::string>("--web-address")));
    if(program.present("--web-dev-path")) {
        auto path = program.get<std::string>("--web-dev-path");
        spdlog::info("Serving static frontend files from {} instead of embedded files", path);
        web_server.set_static_dev_path(path);
    }
    if(!program.get<bool>("--disable-web")) {
        web_server.serve_threaded();
    }

    opentelemetry::Server opentelemetry_server(db, Pistache::Address(program.get<std::string>("--otel-address")));
    opentelemetry_server.serve();

    return 0;
}
