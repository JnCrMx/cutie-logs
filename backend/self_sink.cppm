module;
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <string>

#include <sys/sysinfo.h>
#include <unistd.h>

export module backend.self_sink;

import glaze;
import pqxx;
import spdlog;

import common;
import backend.database;
import backend.utils;

namespace backend {

constexpr auto service_name = common::project_name;
constexpr auto service_version = common::project_version;

static std::string scope{common::project_name};

void collect_attributes(glz::generic& attributes) {
    attributes["process.pid"] = getpid();
    std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe");
    attributes["process.executable.name"] = exe.filename().string();
    attributes["process.executable.path"] = exe.string();
    if(auto command_args = utils::get_process_command_args(); !command_args.empty()) {
        attributes["process.command"] = command_args[0];
        attributes["process.command_args"] = std::move(command_args);
    }

    attributes["host.arch"] = utils::host_arch;
    if(auto f = std::fstream("/etc/machine-id", std::ios::in)) {
        std::string machine_id;
        if(std::getline(f, machine_id)) {
            attributes["host.id"] = std::move(machine_id);
        }
    } else if(auto f = std::fstream("/var/lib/dbus/machine-id", std::ios::in)) {
        std::string machine_id;
        if(std::getline(f, machine_id)) {
            attributes["host.id"] = std::move(machine_id);
        }
    }
    char hostname_buffer[1024];
    gethostname(hostname_buffer, sizeof(hostname_buffer));
    attributes["host.name"] = std::string(hostname_buffer, strnlen(hostname_buffer, 1024));

    attributes["os.type"] = utils::os_type;
    auto os_release = utils::get_os_release();
    if(os_release.contains("BUILD_ID")) {
        attributes["os.build_id"] = os_release["BUILD_ID"];
    }
    if(os_release.contains("PRETTY_NAME")) {
        attributes["os.description"] = os_release["PRETTY_NAME"];
    }
    if(os_release.contains("NAME")) {
        attributes["os.name"] = os_release["NAME"];
    }
    if(os_release.contains("VERSION_ID")) {
        attributes["os.version"] = os_release["VERSION_ID"];
    }
}

template <typename Mutex>
class self_sink : public spdlog::sinks::base_sink<Mutex> {
    public:
        self_sink(database::Database& db) : m_db(db) {
            m_resource_future = m_db.queue_work([this](pqxx::connection& conn){
                glz::generic attributes{
                    {"service.name", service_name},
                    {"service.version", service_version},
                    {"telemetry.sdk.name", "cutie-logs self_sink"},
                    {"telemetry.sdk.version", common::project_version},
                    {"telemetry.sdk.language", "cpp"},
                };
                collect_attributes(attributes);
                m_resource_id = m_db.ensure_resource(conn, attributes);
            });
        }
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            glz::generic attributes = {
                {"thread_id", msg.thread_id},
                {"logger_name", std::string{std::string_view{msg.logger_name}}},
            };
            if(!msg.source.empty()) {
                attributes["source"] = glz::generic{
                    {"filename", msg.source.filename},
                    {"line", msg.source.line},
                    {"funcname", msg.source.funcname},
                };
            }
            std::string body{std::string_view{msg.payload}};

            common::log_severity severity;
            switch(msg.level) {
                case spdlog::level::trace:    severity = common::log_severity::TRACE;       break;
                case spdlog::level::debug:    severity = common::log_severity::DEBUG;       break;
                case spdlog::level::info:     severity = common::log_severity::INFO;        break;
                case spdlog::level::warn:     severity = common::log_severity::WARN;        break;
                case spdlog::level::err:      severity = common::log_severity::ERROR;       break;
                case spdlog::level::critical: severity = common::log_severity::FATAL;       break;
                default:                      severity = common::log_severity::UNSPECIFIED; break;
            }
            auto timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(msg.time);

            if(m_resource_future.valid()) { // make sure the resource exists here
                m_resource_future.get();
            }
            m_db.queue_work([this, attributes = std::move(attributes), body = std::move(body), severity, timestamp](pqxx::connection& conn){
                try {
                    m_db.insert_log(conn, m_resource_id, timestamp, scope, severity, attributes, body);
                } catch(const std::exception& e) {
                    // ignore any exceptions
                }
            });
        }

        void flush_() override {
            // do nothing here, this logger has no concept of flushing
        }

    private:
        database::Database& m_db;
        std::future<void> m_resource_future;
        unsigned int m_resource_id;
};

export using self_sink_mt = self_sink<std::mutex>;
export using self_sink_st = self_sink<spdlog::details::null_mutex>;

}
