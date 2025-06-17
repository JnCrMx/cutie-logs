export module frontend.pages:logs;

import std;
import webpp;
import webxx;
import glaze;

import :page;

import common;
import frontend.assets;
import frontend.components;
import frontend.utils;

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
                    [this](webpp::event) {
                        webpp::coro::submit([this]() -> webpp::coroutine<void> {
                            co_await webpp::coro::next_tick();

                            auto textarea = *webpp::get_element_by_id("stencil_textarea");
                            auto validator = *webpp::get_element_by_id("stencil_validator");
                            stencil_format = textarea["value"].as<std::string>().value_or("");
                            profile.set_data("stencil", stencil_format);
                            if(auto r = common::stencil(stencil_format, *example_entry, stencil_functions)) {
                                validator.inner_text(*r);
                                validator.remove_class("text-error");
                                validator.remove_class("font-bold");

                                textarea.remove_class("textarea-error");
                                textarea.add_class("textarea-success");
                            } else {
                                validator.inner_text(std::format("Stencil invalid: \"{}\"", r.error()));
                                validator.add_class("text-error");
                                validator.add_class("font-bold");

                                textarea.remove_class("textarea-success");
                                textarea.add_class("textarea-error");
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
        std::string build_resources_selector(const std::unordered_map<std::string, bool>& selected_resources) {
            std::string resources_selector{};
            for(const auto& [res, selected] : selected_resources) {
                if(selected) {
                    resources_selector += std::format("{},", res);
                }
            }
            return resources_selector+"0";
        }

        webpp::coroutine<void> run_query() {
            std::string attributes_selector = build_attributes_selector(selected_attributes);
            std::string scopes_selector = build_scopes_selector(selected_scopes);
            std::string resources_selector = build_resources_selector(selected_resources);
            constexpr unsigned int limit = 100;

            auto url = std::format("/api/v1/logs?limit={}&attributes={}&scopes={}&resources={}",
                limit, attributes_selector, scopes_selector, resources_selector);
            auto logs =
                glz::read<common::beve_opts, common::logs_response>(co_await webpp::coro::fetch(url, utils::fetch_options).then(std::mem_fn(&webpp::response::co_bytes)))
                .value_or(common::logs_response{});

            webpp::get_element_by_id("run_button_icon")->remove_class("hidden");
            webpp::get_element_by_id("run_button_loading")->add_class("hidden");

            using namespace Webxx;
            auto list = ul{{_class{"list rounded-box shadow p-4 gap-1"}},
                each(logs.logs, [&](const auto& entry) {
                    auto r = common::stencil(stencil_format, entry, stencil_functions);
                    return li{{_class{"list-item"}},
                        code{{_class{r ? "whitespace-pre" : "whitespace-pre text-error font-bold"}},
                            *r.or_else([](auto err) -> decltype(r) { return std::format("Stencil invalid: \"{}\"", err); })}
                    };
                })
            };
            webpp::get_element_by_id("logs")->inner_html(Webxx::render(list));

            co_return;
        };

        webpp::coroutine<void> download_logs(unsigned int limit = 1000) {
            std::string attributes_selector = build_attributes_selector(selected_attributes);
            std::string scopes_selector = build_scopes_selector(selected_scopes);
            std::string resources_selector = build_resources_selector(selected_resources);

            auto url = std::format("/api/v1/logs/stencil?limit={}&attributes={}&scopes={}&resources={}&stencil={}",
                limit, attributes_selector, scopes_selector, resources_selector, stencil_format);
            webpp::eval("window.open('{}', '_blank');", url);

            co_return;
        };

        void update_stencil() {
            stencil_format = profile.get_data("stencil").value_or(std::string{common::default_stencil});
            webpp::get_element_by_id("stencil_textarea")->set_property("value", stencil_format);
            if(!stencil_format.empty()) {
                webpp::eval("document.getElementById('stencil_textarea').dispatchEvent(new Event('input'));");
            }
        }
        void update_attributes() {
            selected_attributes.clear();
            std::transform(attributes->attributes.begin(), attributes->attributes.end(), std::inserter(selected_attributes, selected_attributes.end()),
                [](const auto& attr) { return std::pair{attr.first, false}; }); // all attributes are deselected by default
            webpp::get_element_by_id("attributes")->inner_html(Webxx::render(
                components::selection<"attributes">("Select Attributes",
                    attributes->attributes, selected_attributes, &profile, attributes->total_logs, true)));
        }
        void update_scopes() {
            selected_scopes.clear();
            std::transform(scopes->scopes.begin(), scopes->scopes.end(), std::inserter(selected_scopes, selected_scopes.end()),
                [](const auto& scope) { return std::pair{scope.first, true}; }); // all scopes are selected by default
            webpp::get_element_by_id("scopes")->inner_html(Webxx::render(
                components::selection<"scopes">("Filter Scopes", scopes->scopes, selected_scopes, &profile, 1, false)));
        }

        std::string resource_name(unsigned int id, const common::log_resource& resource) {
            return resource.guess_name().value_or(std::format("Resource #{}", id));
        }
        void update_resources() {
            transformed_resources.clear();
            for(const auto& [id, e] : resources->resources) {
                transformed_resources[std::to_string(id)] = {resource_name(id, std::get<0>(e)), std::get<1>(e)};
            }
            selected_resources.clear();
            std::transform(transformed_resources.begin(), transformed_resources.end(), std::inserter(selected_resources, selected_resources.end()),
                [](const auto& res) { return std::pair{res.first, false}; }); // all resources are deselected by default
            webpp::get_element_by_id("resources")->inner_html(Webxx::render(
                components::selection_detail<"resources">("Filter Resources", transformed_resources, selected_resources, &profile, 1, false, "resource")));
        }

        profile_data& profile;
        r<common::log_entry>& example_entry;
        r<common::logs_attributes_response>& attributes;
        r<common::logs_scopes_response>& scopes;
        r<common::logs_resources_response>& resources;
        common::advanced_stencil_functions stencil_functions;

        std::string stencil_format;
        std::unordered_map<std::string, bool> selected_attributes, selected_resources, selected_scopes;
        std::unordered_map<std::string, std::tuple<std::string, unsigned int>> transformed_resources;
    public:
        logs(profile_data& profile, r<common::log_entry>& example_entry, r<common::logs_attributes_response>& attributes, r<common::logs_scopes_response>& scopes, r<common::logs_resources_response>& resources,
            std::vector<std::pair<std::string_view, common::mmdb*>> mmdbs)
            : profile(profile), example_entry{example_entry}, attributes{attributes}, scopes{scopes}, resources{resources}, stencil_functions{std::move(mmdbs)}
        {
            profile.add_callback([this](auto&) { if(is_open) { open(); }});
            example_entry.add_callback([this](auto&) { if(is_open) update_stencil(); });
            attributes.add_callback([this](auto&) { if(is_open) update_attributes(); });
            scopes.add_callback([this](auto&) { if(is_open) update_scopes(); });
            resources.add_callback([this](auto&) { if(is_open) update_resources(); });
        }

        void open() override {
            page::open();

            update_stencil();
            update_attributes();
            update_scopes();
            update_resources();
        }

        Webxx::fragment render() override {
            static event_context ctx;
            ctx.clear();

            using namespace Webxx;
            return Webxx::fragment {
                dv{{_class{"flex flex-col m-0 p-0"}},
                    dv{{_class{"flex flex-col md:flex-row gap-4"}},
                        dv{{_id{"attributes"}, _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"attributes">("Select Attributes", attributes->attributes, selected_attributes, &profile, attributes->total_logs, true)},
                        dv{{_id{"resources"},  _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"resources">("Filter Resources", {}, selected_resources, &profile)},
                        dv{{_id{"scopes"},     _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"scopes">("Filter Scopes", scopes->scopes, selected_scopes, &profile, 1, false)},
                    },
                    dv{{_class{"flex flex-col md:flex-row gap-4 items-center"}},
                        dv{{_id{"display"}, _class{"grow w-full"}}, page_display_options()},
                        dv{{_class{"flex flex-row md:flex-col gap-4"}},
                            ctx.on_click(button{{_class{"btn btn-primary"}},
                                span{{_id{"run_button_loading"}, _class{"loading loading-spinner hidden"}}},
                                span{{_id{"run_button_icon"}}, assets::icons::run},
                                "Query"
                            }, [this](webpp::event) {
                                webpp::get_element_by_id("run_button_icon")->add_class("hidden");
                                webpp::get_element_by_id("run_button_loading")->remove_class("hidden");
                                webpp::coro::submit(run_query());
                            }),
                            dv{{_class{"dropdown dropdown-hover dropdown-top md:dropdown-bottom dropdown-end"}},
                                dv{{_class{"btn btn-primary"}, _role{"button"}, _tabindex{"0"}},
                                    span{{_id{"download_button_icon"}}, assets::icons::download},
                                    "Download"
                                },
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
                                                [this, limit = e.second](webpp::event){
                                                    webpp::coro::submit(download_logs(limit));
                                                })};
                                        } else {
                                            return li{ctx.on_click(a{{_class{"btn btn-ghost btn-primary btn-sm justify-start"}}, span{{_class{"size-5"}}}, e.first},
                                                [this, limit = e.second](webpp::event){
                                                    webpp::coro::submit(download_logs(limit));
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
