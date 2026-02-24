module;

#include <spdlog/spdlog.h>
#include <spdlog/cfg/argv.h>
#include <spdlog/cfg/env.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/synchronous_factory.h>
#include <spdlog/sinks/base_sink.h>

#include <fmt/core.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>

export module spdlog;

export namespace spdlog {
    using spdlog::log;
    using spdlog::trace;
    using spdlog::debug;
    using spdlog::info;
    using spdlog::warn;
    using spdlog::error;
    using spdlog::critical;
    using spdlog::set_level;
    using spdlog::should_log;

    using spdlog::logger;
    using spdlog::default_logger;

    using spdlog::synchronous_factory;

    namespace level {
        using spdlog::level::trace;
        using spdlog::level::debug;
        using spdlog::level::info;
        using spdlog::level::warn;
        using spdlog::level::err;
        using spdlog::level::critical;

        using spdlog::level::level_enum;
    }
    namespace cfg {
        using spdlog::cfg::load_env_levels;
        using spdlog::cfg::load_argv_levels;
    }
    namespace sinks {
        using spdlog::sinks::base_sink;
    }
    namespace details {
        using spdlog::details::log_msg;
        using spdlog::details::null_mutex;
    }
};

export namespace fmt {
    using fmt::formatter;
    using fmt::string_view;
    using fmt::arg;
    using fmt::join;
    using fmt::format;

    using fmt::format_context;
    using fmt::format_to;
}
