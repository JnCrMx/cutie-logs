export module frontend.pages:logs;

import std;
import web;
import web_coro;
import webxx;
import glaze;

import :page;

import common;
import frontend.assets;
import frontend.components;

namespace frontend::pages {

export class logs : public page {
    private:
        auto page_display_options() {
            static event_context ctx;
            ctx.clear();

            using namespace Webxx;
            return fieldset{{_class{"fieldset p-4 rounded-box shadow h-full flex flex-col"}},
                legend{{_class{"fieldset-legend"}}, "Display Options"},
                ctx.on_input(textarea{{_id{"stencil_textarea"}, _class{"textarea w-full min-h-[2.5rem]"}, _rows{"1"}, _placeholder{"Log line stencil. Use {...} to insert values."}},},
                    [this](std::string_view) {
                        web::coro::submit_next([this]() -> web::coro::coroutine<void> {
                            stencil_format = *web::get_property("stencil_textarea", "value");
                            web::eval("localStorage.setItem('stencil', '{}'); ''", stencil_format);
                            if(auto r = common::stencil(stencil_format, *example_entry)) {
                                web::set_html("stencil_validator", *r);
                                web::remove_class("stencil_validator", "text-error");
                                web::remove_class("stencil_validator", "font-bold");

                                web::add_class("stencil_textarea", "textarea-success");
                                web::remove_class("stencil_textarea", "textarea-error");
                            } else {
                                web::set_html("stencil_validator", "Stencil invalid: \"{}\"", r.error());
                                web::add_class("stencil_validator", "text-error");
                                web::add_class("stencil_validator", "font-bold");

                                web::add_class("stencil_textarea", "textarea-error");
                                web::remove_class("stencil_textarea", "textarea-success");
                            }
                            co_return;
                        }());
                    }),
                dv{{_class{"fieldset-label"}}, "Preview"},
                textarea{{_id{"stencil_validator"}, _class{"textarea w-full min-h-[2.5rem]"}, _rows{"1"}, _readonly{}}},
            };
        }

        std::string build_attributes_selector(const std::unordered_map<std::string, bool>& selected_attributes) {
            std::string attributes_selector{};
            for(const auto& [attr, selected] : selected_attributes) {
                if(selected) {
                    attributes_selector += std::format("{},", attr);
                }
            }
            if(!attributes_selector.empty()) {
                attributes_selector.pop_back();
            }
            return attributes_selector;
        }
        std::string build_scopes_selector(const std::unordered_map<std::string, bool>& selected_scopes) {
            std::string scopes_selector{};
            for(const auto& [scope, selected] : selected_scopes) {
                if(selected) {
                    scopes_selector += std::format("{},", scope.empty() ? "<empty>" : scope);
                }
            }
            return scopes_selector+"<dummy>";
        }

        web::coro::coroutine<void> run_query() {
            std::string attributes_selector = build_attributes_selector(selected_attributes);
            std::string scopes_selector = build_scopes_selector(selected_scopes);
            constexpr unsigned int limit = 100;

            auto url = std::format("/api/v1/logs?limit={}&attributes={}&scopes={}", limit, attributes_selector, scopes_selector);
            auto logs =
                glz::read_beve<common::logs_response>(co_await web::coro::fetch(url))
                .value_or(common::logs_response{});

            web::remove_class("run_button_icon", "hidden");
            web::add_class("run_button_loading", "hidden");

            using namespace Webxx;
            auto list = ul{{_class{"list rounded-box shadow p-4 gap-1"}},
                each(logs.logs, [&](const auto& entry) {
                    auto r = common::stencil(stencil_format, entry);
                    return li{{_class{"list-item"}},
                        code{{_class{r ? "whitespace-pre" : "whitespace-pre text-error font-bold"}},
                            *r.or_else([](auto err) -> decltype(r) { return std::format("Stencil invalid: \"{}\"", err); })}
                    };
                })
            };
            web::set_html("logs", Webxx::render(list));

            co_return;
        };

        web::coro::coroutine<void> download_logs(unsigned int limit = 1000) {
            std::string attributes_selector = build_attributes_selector(selected_attributes);
            std::string scopes_selector = build_scopes_selector(selected_scopes);

            auto url = std::format("/api/v1/logs/stencil?limit={}&attributes={}&scopes={}&stencil={}", limit, attributes_selector, scopes_selector, stencil_format);
            web::eval("window.open('{}', '_blank');", url);

            co_return;
        };

        void update_example_entry() {
            stencil_format = web::eval("let stencil = localStorage.getItem('stencil'); if(stencil === null) {stencil = '';}; stencil");
            web::set_property("stencil_textarea", "value", stencil_format);
            if(!stencil_format.empty()) {
                web::eval("document.getElementById('stencil_textarea').dispatchEvent(new Event('input'));");
            }
        }
        void update_attributes() {
            std::transform(attributes->attributes.begin(), attributes->attributes.end(), std::inserter(selected_attributes, selected_attributes.end()),
                [](const auto& attr) { return std::pair{attr.first, false}; }); // all attributes are deselected by default
            web::set_html("attributes", Webxx::render(components::selection<"attributes">("Select Attributes", attributes->attributes, selected_attributes, attributes->total_logs, true)));
        }
        void update_scopes() {
            std::transform(scopes->scopes.begin(), scopes->scopes.end(), std::inserter(selected_scopes, selected_scopes.end()),
                [](const auto& scope) { return std::pair{scope.first, true}; }); // all scopes are selected by default
            web::set_html("scopes", Webxx::render(components::selection<"scopes">("Filter Scopes", scopes->scopes, selected_scopes, 1, false)));
        }

        r<common::log_entry>& example_entry;
        r<common::logs_attributes_response>& attributes;
        r<common::logs_scopes_response>& scopes;

        std::string stencil_format;
        std::unordered_map<std::string, bool> selected_attributes, selected_resources, selected_scopes;
    public:
        logs(r<common::log_entry>& example_entry, r<common::logs_attributes_response>& attributes, r<common::logs_scopes_response>& scopes)
            : example_entry{example_entry}, attributes{attributes}, scopes{scopes}
        {
            example_entry.add_callback([this](auto&) { update_example_entry(); });
            attributes.add_callback([this](auto&) { update_attributes(); });
            scopes.add_callback([this](auto&) { update_scopes(); });
        }

        Webxx::fragment render() override {
            static event_context ctx;
            ctx.clear();

            using namespace Webxx;
            return Webxx::fragment {
                dv{{_class{"flex flex-col m-0 p-0"}},
                    dv{{_class{"flex flex-col md:flex-row gap-4"}},
                        dv{{_id{"attributes"}, _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"attributes">("Select Attributes", attributes->attributes, selected_attributes, attributes->total_logs, true)},
                        dv{{_id{"resources"},  _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"resources">("Filter Resources", {}, selected_resources)},
                        dv{{_id{"scopes"},     _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"scopes">("Filter Scopes", scopes->scopes, selected_scopes, 1, false)},
                    },
                    dv{{_class{"flex flex-col md:flex-row gap-4 items-center"}},
                        dv{{_id{"display"}, _class{"grow w-full"}}, page_display_options()},
                        dv{{_class{"flex flex-row md:flex-col gap-4"}},
                            ctx.on_click(button{{_class{"btn btn-primary"}},
                                span{{_id{"run_button_loading"}, _class{"loading loading-spinner hidden"}}},
                                span{{_id{"run_button_icon"}}, assets::icons::run},
                                "Query"
                            }, [this](std::string_view) {
                                web::add_class("run_button_icon", "hidden");
                                web::remove_class("run_button_loading", "hidden");
                                web::coro::submit(run_query());
                            }),
                            dv{{_class{"dropdown dropdown-hover dropdown-top md:dropdown-bottom dropdown-end"}},
                                ctx.on_click(dv{{_class{"btn btn-primary"}, _role{"button"}, _tabindex{"0"}},
                                    span{{_id{"download_button_icon"}}, assets::icons::download},
                                    "Download"
                                }, [this](std::string_view) {
                                    web::coro::submit(download_logs());
                                }),
                                ul{{_class{"dropdown-content menu bg-base-300 rounded-box z-1 w-full p-1 shadow-sm"}},
                                    each(std::array{
                                        std::make_pair<std::string_view, unsigned int>("100 entries", 100),
                                        std::make_pair<std::string_view, unsigned int>("1k entries", 1000),
                                        std::make_pair<std::string_view, unsigned int>("10k entries", 10000),
                                        std::make_pair<std::string_view, unsigned int>("100k entries", 100000),
                                        std::make_pair<std::string_view, unsigned int>("1M entries", 1000000),
                                    }, [&](const auto& e) {
                                        constexpr unsigned int warning_limit = 2500;
                                        if(e.second > warning_limit) {
                                            return li{ctx.on_click(a{{_class{"btn btn-ghost btn-primary btn-sm justify-start text-warning"}}, assets::icons::warning, e.first},
                                                [this, limit = e.second](std::string_view){
                                                    web::coro::submit(download_logs(limit));
                                                })};
                                        } else {
                                            return li{ctx.on_click(a{{_class{"btn btn-ghost btn-primary btn-sm justify-start"}}, span{{_class{"size-5"}}}, e.first},
                                                [this, limit = e.second](std::string_view){
                                                    web::coro::submit(download_logs(limit));
                                                })};
                                        }
                                    })
                                }
                            }
                        }
                    },
                    dv{{_id{"logs"}, _class{"mt-4 overflow-x-auto"}}},
                }
            };
        }
};

}
