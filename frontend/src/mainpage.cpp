import std;
import webpp;
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

using page_tuple = std::tuple<std::string_view, std::string_view, std::string_view, pages::page*>;
static std::array all_pages = {
    page_tuple{"logs", "Text View", assets::icons::text_view, &logs_page},
    page_tuple{"table", "Table View", assets::icons::table_view, &table_page},
//    page_tuple{"analysis", "Analysis", assets::icons::analysis, &logs_page},
};

static pages::page* current_page = nullptr;

Webxx::dv page_stats(const stats_data& data);

auto refresh() -> webpp::coroutine<void> {
    auto refresh_button = *webpp::get_element_by_id("refresh-button");
    refresh_button.add_class("btn-disabled");
    refresh_button.add_class("*:animate-spin");

    auto attributes_future = webpp::coro::fetch("/api/v1/logs/attributes").then(std::mem_fn(&webpp::response::co_bytes));
    auto scopes_future = webpp::coro::fetch("/api/v1/logs/scopes").then(std::mem_fn(&webpp::response::co_bytes));
    auto resources_future = webpp::coro::fetch("/api/v1/logs/resources").then(std::mem_fn(&webpp::response::co_bytes));
    auto example_future = webpp::coro::fetch("/api/v1/logs?limit=1&attributes=*").then(std::mem_fn(&webpp::response::co_bytes));

    attributes = glz::read_beve<common::logs_attributes_response>(co_await attributes_future).value_or(common::logs_attributes_response{});
    scopes = glz::read_beve<common::logs_scopes_response>(co_await scopes_future).value_or(common::logs_scopes_response{});
    resources = glz::read_beve<common::logs_resources_response>(co_await resources_future).value_or(common::logs_resources_response{});

    auto resource_modals = Webxx::each(resources->resources, [](auto& e){
        return components::resource_modal{std::get<0>(e), std::get<0>(std::get<1>(e))};
    });
    webpp::get_element_by_id("modals-resources")->inner_html(Webxx::render(resource_modals));

    auto example = glz::read_beve<common::logs_response>(co_await example_future).value_or(common::logs_response{});
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
    webpp::get_element_by_id("stats")->inner_html(Webxx::render(page_stats(stats)));
    webpp::get_element_by_id("stats")->add_class("animate-pulse");

    // in theory we don't need this, because we rerender this area of the page ("stats") anyway
    refresh_button.remove_class("btn-disabled");
    refresh_button.remove_class("*:animate-spin");
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
            [](webpp::event) {
                webpp::coro::submit(refresh());
            }
        )
    };
}

void switch_page(pages::page* page, std::string_view id) {
    if(current_page == page) {
        return;
    }

    if(current_page) {
        current_page->close();
    }
    for(auto [page_id, name, icon, page_ptr] : all_pages) {
        std::string menu_id = "menu_" + std::string{page_id};
        webpp::get_element_by_id(menu_id)->remove_class("menu-active");
    }
    std::string menu_id = "menu_" + std::string{id};
    webpp::get_element_by_id(menu_id)->add_class("menu-active");

    webpp::get_element_by_id("page")->inner_html(Webxx::render(page->render()));
    webpp::set_timeout(std::chrono::milliseconds(0), [page](){page->open();});

    webpp::eval("document.location.hash = '#{}';", id);
    webpp::eval("localStorage.setItem('page', '{}');", id);
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
                    each(all_pages, [](auto& page) {
                        auto [id, name, icon, page_ptr] = page;
                        std::string menu_id = "menu_" + std::string{id};
                        return li{ctx.on_click(a{{_id{menu_id}}, icon, name}, [page_ptr, id](auto){switch_page(page_ptr, id);})};
                    }),
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

bool try_switch_page(std::string_view id) {
    for(auto [page_id, name, icon, page_ptr] : all_pages) {
        if(page_id == id) {
            switch_page(page_ptr, id);
            return true;
        }
    }
    return false;
}

void auto_select_page() {
    std::string page = webpp::eval("let page = document.location.hash; if(page === null) {page = '';}; page")["result"]
        .as<std::string>().value_or("");
    if(!page.empty()) {
        page = page.substr(1);
    }
    if(page == "none") {
        return;
    }
    if(try_switch_page(page)) {
        return;
    }

    page = webpp::eval("let page = localStorage.getItem('page'); if(page === null) {page = '';}; page")["result"]
        .as<std::string>().value_or("");
    if(page == "none") {
        return;
    }
    if(try_switch_page(page)) {
        return;
    }

    if(!all_pages.empty()) {
        auto [page_id, name, icon, page_ptr] = all_pages.front();
        switch_page(page_ptr, page_id);
    }
}

[[clang::export_name("main")]]
int main() {
    std::string theme = webpp::eval("let theme = localStorage.getItem('theme'); if(theme === null) {theme = '';}; theme")["result"]
        .as<std::string>().value_or("");
    if(theme.empty()) {
        theme = "light";
        webpp::eval("localStorage.setItem('theme', 'light'); ''");
    }

    auto main = *webpp::get_element_by_id("main");
    main.set_property("data-theme", theme);
    main.inner_html(Webxx::render(page(theme)));

    auto_select_page();

    webpp::coro::submit(refresh());

    return 0;
}

}
