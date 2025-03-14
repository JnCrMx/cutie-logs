import pistache;
import pqxx;
import argparse;
import spdlog;

import common;
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
    program.add_argument("--disable-web")
        .help("Disable the web interface")
        .flag();

    try {
        program.parse_args(argc, argv);
    } catch(const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 2;
    }

    using namespace backend;
    spdlog::info("Starting {} version {}", common::project_name, common::project_version);

    web::Server webServer(Pistache::Address(program.get<std::string>("--web-address")));
    if(!program.get<bool>("--disable-web")) {
        webServer.serveThreaded();
    }

    opentelemetry::Server opentelemetryServer(Pistache::Address(program.get<std::string>("--otel-address")));
    opentelemetryServer.serve();

    return 0;
}
