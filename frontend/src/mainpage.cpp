import std;
import web;
import web_coro;
import webxx;
import glaze;

import common;

namespace Webxx {
    constexpr static char customDataTipAttr[] = "data-tip";
    using _dataTip = attr<customDataTipAttr>;

    constexpr static char customDataThemeAttr[] = "data-theme";
    using _dataTheme = attr<customDataThemeAttr>;

    constexpr static char customDataCallbackAttr[] = "data-callback";
    using _dataCallback = attr<customDataCallbackAttr>;

    constexpr static char customAriaLabelAttr[] = "aria-label";
    using _ariaLabel = attr<customAriaLabelAttr>;

    constexpr static char onClickAttr[] = "onclick";
    using _onClick = attr<onClickAttr>;

    constexpr static char onInputAttr[] = "oninput";
    using _onInput = attr<onInputAttr>;

    constexpr static char onChangeAttr[] = "onchange";
    using _onChange = attr<onChangeAttr>;
}

class event_context {
    public:
        auto on_click(auto el, web::event_callback&& cb) {
            return on_event<Webxx::_onClick>(std::move(el), std::move(cb));
        }
        auto on_input(auto el, web::event_callback&& cb) {
            return on_event<Webxx::_onInput>(std::move(el), std::move(cb));
        }
        auto on_change(auto el, web::event_callback&& cb) {
            return on_event<Webxx::_onChange>(std::move(el), std::move(cb));
        }

        template<typename EventAttribute>
        auto on_event(auto el, web::event_callback&& cb) {
            auto& data = callbacks.emplace_back(std::make_unique<web::callback_data>(std::move(cb), false));
            el.data.attributes.push_back(Webxx::_dataCallback{std::format("{}", reinterpret_cast<std::uintptr_t>(data.get()))});
            el.data.attributes.push_back(EventAttribute{"handleEvent(this, event);"});
            return el;
        }

        void clear() {
            callbacks.clear();
        }
    private:
        std::vector<std::unique_ptr<web::callback_data>> callbacks;
};

namespace assets {
    namespace icons {
        constexpr static char run[] = {
            #embed "assets/icons/run.svg"
        };
    }
}

static common::log_entry example_entry;
static std::unordered_map<std::string, bool> selected_attributes, selected_resources, selected_scopes;
static std::string stencil_format;

web::coro::coroutine<void> run_query() {
    std::string attributes_selector{};
    for(const auto& [attr, selected] : selected_attributes) {
        if(selected) {
            attributes_selector += std::format("{},", attr);
        }
    }
    if(!attributes_selector.empty()) {
        attributes_selector.pop_back();
    }

    auto logs =
        glz::read_beve<common::logs_response>(co_await web::coro::fetch("/api/v1/logs?limit=100&attributes="+attributes_selector))
        .value_or(common::logs_response{});

    web::remove_class("run_button_icon", "hidden");
    web::add_class("run_button_loading", "hidden");

    using namespace Webxx;
    auto list = ul{{_class{"list rounded-box shadow p-4 gap-1"}},
        each(logs.logs, [&](const auto& entry) {
            auto r = common::stencil(stencil_format, entry);
            return li{{_class{"list-item"}},
                code{{_class{r ? "" : "text-error font-bold"}}, *r.or_else([](auto err) -> decltype(r) { return std::format("Stencil invalid: \"{}\"", err); })}
            };
        })
    };
    web::set_html("logs", render(list));

    co_return;
};

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
template<glz::string_literal what>
auto page_selection(const std::unordered_map<std::string, unsigned int>& attributes,
    std::unordered_map<std::string, bool>& output, unsigned int total = 1, bool show_percent = false)
{
    static event_context ctx;
    using namespace Webxx;
    auto search_id = std::format("select_{}_search", what.sv());
    return fieldset{{_class{"fieldset p-4 rounded-box shadow h-full flex flex-col"}},
        legend{{_class{"fieldset-legend"}}, what.sv()},
        label{{_class{"input w-full mb-2"}},
            ctx.on_input(input{{_id{search_id}, _type{"search"}, _class{"grow"}, _placeholder{"Search..."}}},
                [search_id, attributes](std::string_view) {
                    auto search = web::get_property(search_id, "value").value_or("");
                    std::transform(search.begin(), search.end(), search.begin(), [](char c){return std::tolower(c);});

                    for(const auto& [attr, _] : attributes) {
                        std::string name = attr;
                        std::transform(name.begin(), name.end(), name.begin(), [](char c){return std::tolower(c);});

                        auto id = std::format("select_{}_entry_{}", what.sv(), attr);
                        if(name.find(search) != std::string::npos) {
                            web::remove_class(id, "hidden");
                        } else {
                            web::add_class(id, "hidden");
                        }
                    }
                })
        },
        dv{{_class{"overflow-y-auto h-full flex flex-col gap-1"}},
            each(attributes, [total, show_percent, &output](const auto& attr) {
                auto id = std::format("select_{}_entry_{}", what.sv(), attr.first);
                auto checkbox_id = std::format("select_{}_entry_{}_checkbox", what.sv(), attr.first);
                return fragment{label{{_id{id}, _class{"fieldset-label"}},
                    ctx.on_change(input{{_id{checkbox_id}, _type{"checkbox"}, _class{"checkbox"}, _ariaLabel{attr.first}, _value{attr.first}}},
                        [checkbox_id, attr, &output](std::string_view event){
                            bool checked = web::get_property(checkbox_id, "checked") == "true";
                            output[attr.first] = checked;
                        }),
                    show_percent ?
                        std::format("{} ({}%)", attr.first, attr.second*100/total) :
                        std::format("{} ({})", attr.first, attr.second)
                }};
            })
        }
    };
}
auto page_display_options() {
    static event_context ctx;
    ctx.clear();

    using namespace Webxx;
    return fieldset{{_class{"fieldset p-4 rounded-box shadow h-full flex flex-col"}},
        legend{{_class{"fieldset-legend"}}, "Display Options"},
        ctx.on_input(textarea{{_id{"stencil_textarea"}, _class{"textarea w-full min-h-[2.5rem]"}, _rows{"1"}, _placeholder{"Log line stencil. Use {...} to insert values."}},},
            [](std::string_view) {
                web::coro::submit_next([]() -> web::coro::coroutine<void> {
                    stencil_format = *web::get_property("stencil_textarea", "value");
                    web::eval("localStorage.setItem('stencil', '{}'); ''", stencil_format);
                    if(auto r = common::stencil(stencil_format, example_entry)) {
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
auto page_logs() {
    using namespace Webxx;
    return
        "Logs will be displayed here"
    ;
}
auto page(std::string_view current_theme) {
    using namespace Webxx;

    constexpr static std::array themes = {
        "light", "dark", "cupcake", "bumblebee", "emerald",
        "corporate", "synthwave", "retro", "cyberpunk", "valentine",
        "halloween", "garden", "forest", "aqua", "lofi",
        "pastel", "fantasy", "wireframe", "black", "luxury",
        "dracula", "cmyk", "autumn", "business", "acid",
        "lemonade", "night", "coffee", "winter", "dim",
        "nord", "sunset", "caramellatte", "abyss", "silk",
    };
    struct ThemeButton : component<ThemeButton> {

        ThemeButton(event_context& ctx, std::string_view current) : component<ThemeButton>{
            dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Themes"}},
                dv{{_class{"dropdown dropdown-end dropdown-bottom"}},
                    dv{{_class{"btn btn-square m-1"}, _tabindex{"0"}, _role{"button"}}},
                    ul{{_class{"dropdown-content bg-base-300 rounded-box z-[1] w-52 p-2 shadow-2xl h-80 overflow-y-auto"}, _tabindex{"0"}},
                        each(themes, [&ctx, current](const auto& theme) {
                            auto cb = [theme](std::string_view) {
                                web::eval("localStorage.setItem('theme', '{}'); ''", theme);
                                web::set_attribute("main", "data-theme", theme);
                            };
                            if(current == theme) {
                                return li{ctx.on_click(input{{_type{"radio"}, _name{"theme-dropdown"},
                                    _class{"theme-controller btn btn-block btn-ghost btn-sm justify-start"},
                                    _ariaLabel{theme}, _value{theme}, _checked{}}}, cb)};
                            } else {
                                return li{ctx.on_click(input{{_type{"radio"}, _name{"theme-dropdown"},
                                    _class{"theme-controller btn btn-block btn-ghost btn-sm justify-start"},
                                    _ariaLabel{theme}, _value{theme}}}, cb)};
                            }
                        })
                    }
                }}
        } {}
    };

    static event_context ctx;
    ctx.clear();
    return fragment {
        dv{{_class{"flex flex-row items-baseline pt-4 pb-4 sticky top-0 z-10 bg-base-200"}},
            dv{{_class{"text-2xl font-bold ml-4"}}, common::project_name},
            dv{{_class{"text-1xl font-bold ml-2"}}, "version ", common::project_version},
            dv{{_id{"stats"}, _class{"ml-2 mr-2 grow flex flex-row justify-center items-center"}}, page_stats({})},
            dv{{_class{"flex items-center mr-4"}},
                ThemeButton{ctx, current_theme}
            }
        },
        dv{{_class{"w-7xl mx-auto mt-2"}},
            dv{{_class{"flex flex-row gap-4 h-60"}},
                dv{{_id{"attributes"}, _class{"basis-0 grow"}}, page_selection<"Select Attributes">({}, selected_attributes)},
                dv{{_id{"resources"}, _class{"basis-0 grow"}}, page_selection<"Filter Resources">({}, selected_resources)},
                dv{{_id{"scopes"}, _class{"basis-0 grow"}}, page_selection<"Filter Scopes">({}, selected_scopes)},
            },
            dv{{_class{"flex flex-row gap-4 items-center"}},
                dv{{_id{"display"}, _class{"grow"}}, page_display_options()},
                dv{{_class{"flex flex-col gap-4"}},
                    ctx.on_click(button{{_class{"btn btn-primary"}},
                        span{{_id{"run_button_loading"}, _class{"loading loading-spinner hidden"}}},
                        span{{_id{"run_button_icon"}}, assets::icons::run},
                        "Run"
                    }, [](std::string_view) {
                        web::add_class("run_button_icon", "hidden");
                        web::remove_class("run_button_loading", "hidden");
                        web::coro::submit(run_query());
                    })
                }
            },
            dv{{_id{"logs"}, _class{"mt-4"}}, page_logs()},
        }
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

    web::coro::submit([]()->web::coro::coroutine<void> {
        auto attributes =
            glz::read_beve<common::logs_attributes_response>(co_await web::coro::fetch("/api/v1/logs/attributes"))
            .value_or(common::logs_attributes_response{});
        web::set_html("attributes", Webxx::render(page_selection<"Select Attributes">(attributes.attributes, selected_attributes, attributes.total_logs, true)));

        auto scopes =
            glz::read_beve<common::logs_scopes_response>(co_await web::coro::fetch("/api/v1/logs/scopes"))
            .value_or(common::logs_scopes_response{});
        web::set_html("scopes", Webxx::render(page_selection<"Filter Scopes">(scopes.scopes, selected_scopes, 1, false)));

        stats_data stats;
        stats.total_attributes = attributes.attributes.size();
        stats.total_logs = attributes.total_logs;
        stats.total_scopes = scopes.scopes.size();
        web::set_html("stats", Webxx::render(page_stats(stats)));

        std::string attributes_selector{};
        for(const auto& [attr, _] : attributes.attributes) {
            attributes_selector += std::format("{},", attr);
            selected_attributes[attr] = false;
        }
        if(!attributes_selector.empty()) {
            attributes_selector.pop_back();
        }
        auto example =
            glz::read_beve<common::logs_response>(co_await web::coro::fetch("/api/v1/logs?limit=1&attributes="+attributes_selector))
            .value_or(common::logs_response{});
        if(!example.logs.empty()) {
            example_entry = example.logs.front();
        }

        stencil_format = web::eval("let stencil = localStorage.getItem('stencil'); if(stencil === null) {stencil = '';}; stencil");
        web::set_property("stencil_textarea", "value", stencil_format);
        if(!stencil_format.empty()) {
            web::eval("document.getElementById('stencil_textarea').dispatchEvent(new Event('input'));");
        }

        co_return;
    }());

    return 0;
}
