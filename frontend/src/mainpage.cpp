import std;
import web;
import web_coro;
import webxx;
import glaze;

import common;
import frontend.assets;
import frontend.components;
import frontend.pages;

namespace frontend {

static r<common::log_entry> example_entry;
static r<common::logs_attributes_response> attributes;
static r<common::logs_scopes_response> scopes;

static pages::logs logs_page(example_entry, attributes, scopes);

struct stats_data {
    unsigned int total_logs{};
    unsigned int total_attributes{};
    unsigned int total_resources{};
    unsigned int total_scopes{};
};
auto page_stats(const stats_data& data) {
    using namespace Webxx;
    return dv{{_class{"stats shadow"}},
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Log Entries"},
            dv{{_class{"stat-value"}}, std::to_string(data.total_logs)}
        },
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Attributes"},
            dv{{_class{"stat-value"}}, std::to_string(data.total_attributes)}
        },
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Resources"},
            dv{{_class{"stat-value"}}, std::to_string(data.total_resources)}
        },
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Scopes"},
            dv{{_class{"stat-value"}}, std::to_string(data.total_scopes)}
        }
    };
}

auto page_logs() {
    using namespace Webxx;
    return
        "Logs will be displayed here"
    ;
}

auto page(std::string_view current_theme) {
    using namespace Webxx;

    static event_context ctx;
    ctx.clear();
    return fragment {
        dv{{_class{"flex flex-row items-baseline pt-4 pb-4 sticky top-0 z-10 bg-base-200"}},
            dv{{_class{"text-2xl font-bold ml-4"}}, common::project_name},
            dv{{_class{"text-1xl font-bold ml-2"}}, "version ", common::project_version},
            dv{{_id{"stats"}, _class{"ml-2 mr-2 grow flex flex-row justify-center items-center"}}, page_stats({})},
            dv{{_class{"flex items-center mr-4"}},
                components::theme_button{ctx, current_theme}
            }
        },
        dv{{_class{"w-7xl mx-auto mt-2"}, {_id{"page"}}}}
    };
}

[[clang::export_name("main")]]
int main() {
    std::string theme = web::eval("let theme = localStorage.getItem('theme'); if(theme === null) {theme = '';}; theme");
    if(theme.empty()) {
        theme = "light";
        web::eval("localStorage.setItem('theme', 'light'); ''");
    }
    web::set_attribute("main", "data-theme", theme);
    web::set_html("main", Webxx::render(page(theme)));
    web::set_html("page", Webxx::render(logs_page.render()));

    web::coro::submit([]()->web::coro::coroutine<void> {
        attributes =
            glz::read_beve<common::logs_attributes_response>(co_await web::coro::fetch("/api/v1/logs/attributes"))
            .value_or(common::logs_attributes_response{});
        scopes =
            glz::read_beve<common::logs_scopes_response>(co_await web::coro::fetch("/api/v1/logs/scopes"))
            .value_or(common::logs_scopes_response{});

        stats_data stats;
        stats.total_attributes = attributes->attributes.size();
        stats.total_logs = attributes->total_logs;
        stats.total_scopes = scopes->scopes.size();
        web::set_html("stats", Webxx::render(page_stats(stats)));

        std::string attributes_selector{};
        for(const auto& [attr, _] : attributes->attributes) {
            attributes_selector += std::format("{},", attr);
        }
        if(!attributes_selector.empty()) {
            attributes_selector.pop_back();
        }
        auto example =
            glz::read_beve<common::logs_response>(co_await web::coro::fetch("/api/v1/logs?limit=1&attributes="+attributes_selector))
            .value_or(common::logs_response{});
        if(!example.logs.empty()) {
            example_entry = example.logs.front();
        } else {
            example_entry = common::log_entry{};
        }

        co_return;
    }());

    return 0;
}

}
