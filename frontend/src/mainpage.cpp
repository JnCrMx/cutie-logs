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

struct stats_data {
    unsigned int total_logs{};
    unsigned int total_attributes{};
    unsigned int total_resources{};
    unsigned int total_scopes{};
};

static r<common::log_entry> example_entry;
static r<common::logs_attributes_response> attributes;
static r<common::logs_scopes_response> scopes;
static r<common::logs_resources_response> resources;

static pages::logs logs_page(example_entry, attributes, scopes, resources);
static pages::table table_page(attributes, scopes, resources);

static pages::page* current_page = nullptr;

Webxx::dv page_stats(const stats_data& data);

auto refresh() -> web::coro::coroutine<void> {
    web::add_class("refresh-button", "btn-disabled");
    web::add_class("refresh-button", "*:animate-spin");

    attributes =
        glz::read_beve<common::logs_attributes_response>(co_await web::coro::fetch("/api/v1/logs/attributes"))
        .value_or(common::logs_attributes_response{});
    scopes =
        glz::read_beve<common::logs_scopes_response>(co_await web::coro::fetch("/api/v1/logs/scopes"))
        .value_or(common::logs_scopes_response{});
    resources =
        glz::read_beve<common::logs_resources_response>(co_await web::coro::fetch("/api/v1/logs/resources"))
        .value_or(common::logs_resources_response{});

    auto resource_modals = Webxx::each(resources->resources, [](auto& e){
        return components::resource_modal{std::get<0>(e), std::get<0>(std::get<1>(e))};
    });
    web::set_html("modals-resources", Webxx::render(resource_modals));

    auto example =
        glz::read_beve<common::logs_response>(co_await web::coro::fetch("/api/v1/logs?limit=1&attributes=*"))
        .value_or(common::logs_response{});
    if(!example.logs.empty()) {
        example_entry = example.logs.front();
    } else {
        example_entry = common::log_entry{};
    }

    stats_data stats;
    stats.total_attributes = attributes->attributes.size();
    stats.total_logs = attributes->total_logs;
    stats.total_scopes = scopes->scopes.size();
    stats.total_resources = resources->resources.size();
    web::set_html("stats", Webxx::render(page_stats(stats)));

    // in theory we don't need this, because we rerender this area of the page ("stats") anyway
    web::remove_class("refresh-button", "btn-disabled");
    web::remove_class("refresh-button", "*:animate-spin");
    co_return;
}

Webxx::dv page_stats(const stats_data& data) {
    static event_context ctx;
    ctx.clear();

    using namespace Webxx;
    return dv{{_class{"stats stats-vertical md:stats-horizontal shadow items-center"}},
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
        },
        ctx.on_click(button{{_id{"refresh-button"}, _class{"btn btn-square size-20 p-2 m-2"}}, assets::icons::refresh},
            [](std::string_view) {
                web::coro::submit(refresh());
            }
        )
    };
}

void switch_page(pages::page* page, std::string_view menu_id) {
    if(current_page == page) {
        return;
    }

    if(current_page) {
        current_page->close();
    }
    for(auto id : {"menu_logs", "menu_table", "menu_analysis"}) {
        web::remove_class(id, "menu-active");
    }
    web::add_class(menu_id, "menu-active");

    web::set_html("page", Webxx::render(page->render()));
    web::set_timeout(std::chrono::milliseconds(0), [page](auto){page->open();});
}

auto page(std::string_view current_theme) {
    using namespace Webxx;

    static event_context ctx;
    ctx.clear();
    return fragment {
        dv{{_class{"flex flex-row items-baseline pt-4 pb-4 md:sticky top-0 z-10 bg-base-200"}},
            dv{{_class{"flex flex-col m-0 px-4 w-full md:w-auto gap-2"}},
                dv{{_class{"flex flex-row items-baseline m-0 p-0"}},
                    dv{{_class{"text-2xl font-bold"}}, common::project_name},
                    dv{{_class{"text-1xl font-bold ml-2"}}, "version ", common::project_version},
                },
                ul{{_class{"menu bg-base-300 md:menu-horizontal rounded-box w-full md:w-auto"}},
                    li{ctx.on_click(a{{_id{"menu_logs"}}, assets::icons::text_view, "Text View"}, [](auto){switch_page(&logs_page, "menu_logs");})},
                    li{ctx.on_click(a{{_id{"menu_table"}}, assets::icons::table_view, "Table View"}, [](auto){switch_page(&table_page, "menu_table");})},
                    li{ctx.on_click(a{{_id{"menu_analysis"}}, assets::icons::analysis, "Analysis"}, [](auto){switch_page(&logs_page, "menu_analysis");})},
                }
            },
            dv{{_id{"stats"}, _class{"ml-2 mr-2 grow flex flex-row justify-center items-center"}}, page_stats({})},
            dv{{_class{"flex items-center mr-4"}},
                components::theme_button{ctx, current_theme}
            }
        },
        dv{{_class{"w-full mx-auto my-2 px-4 md:w-auto md:mx-[10vw]"}, {_id{"page"}}}},
        dv{{_id{"modals"}},
            dv{{_id{"modals-resources"}}}
        },
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
    switch_page(&logs_page, "menu_logs");

    web::coro::submit(refresh());

    return 0;
}

}
