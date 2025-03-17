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
}

class event_context {
    public:
        auto on_click(auto el, web::event_callback&& cb) {
            return on_event<Webxx::_onClick>(std::move(el), std::move(cb));
        }
        auto on_input(auto el, web::event_callback&& cb) {
            return on_event<Webxx::_onInput>(std::move(el), std::move(cb));
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
auto page_selection(std::string_view what, const std::unordered_map<std::string, unsigned int>& attributes, unsigned int total = 1, bool show_percent = false) {
    using namespace Webxx;
    return fieldset{{_class{"fieldset p-4 rounded-box shadow h-full flex flex-col"}},
        legend{{_class{"fieldset-legend"}}, what},
        label{{_class{"input w-full mb-2"}},
            input{{_type{"search"}, _class{"grow"}, _placeholder{"Search..."}}},
        },
        dv{{_class{"overflow-y-auto h-full flex flex-col gap-1"}},
            each(attributes, [total, show_percent](const auto& attr) {
                return fragment{label{{_class{"fieldset-label"}},
                    input{{_type{"checkbox"}, _class{"checkbox"}, _ariaLabel{attr.first}, _value{attr.first}}},
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
        ctx.on_input(textarea{{_id{"stencil_textarea"}, _class{"textarea h-24 w-full"}, _placeholder{"Log line stencil. Use {...} to insert values."}},},
            [](std::string_view) {
                web::coro::submit_next([]() -> web::coro::coroutine<void> {
                    auto val = *web::get_property("stencil_textarea", "value");
                    common::log_entry test{};
                    if(auto r = common::stencil(val, test)) {
                        web::set_html("stencil_validator", "Stencil valid.");
                        web::remove_class("stencil_validator", "text-error");

                        web::add_class("stencil_textarea", "textarea-success");
                        web::remove_class("stencil_textarea", "textarea-error");
                    } else {
                        web::set_html("stencil_validator", "Stencil invalid: \"{}\"", r.error());
                        web::add_class("stencil_validator", "text-error");

                        web::add_class("stencil_textarea", "textarea-error");
                        web::remove_class("stencil_textarea", "textarea-success");
                    }
                    co_return;
                }());
            }),
        dv{{_id{"stencil_validator"}, _class{"fieldset-label"}}, "Stencil valid."}
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
                dv{{_id{"attributes"}, _class{"basis-0 grow"}}, page_selection("Select Attributes", {})},
                dv{{_id{"resources"}, _class{"basis-0 grow"}}, page_selection("Filter Resources", {})},
                dv{{_id{"scopes"}, _class{"basis-0 grow"}}, page_selection("Filter Scopes", {})},
            },
            dv{{_id{"display"}, _class{"w-full"}}, page_display_options()},
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
        web::set_html("attributes", Webxx::render(page_selection("Select Attributes", attributes.attributes, attributes.total_logs, true)));

        auto scopes =
            glz::read_beve<common::logs_scopes_response>(co_await web::coro::fetch("/api/v1/logs/scopes"))
            .value_or(common::logs_scopes_response{});
        web::set_html("scopes", Webxx::render(page_selection("Filter Scopes", scopes.scopes, 1, false)));

        stats_data stats;
        stats.total_attributes = attributes.attributes.size();
        stats.total_logs = attributes.total_logs;
        stats.total_scopes = scopes.scopes.size();
        web::set_html("stats", Webxx::render(page_stats(stats)));

        co_return;
    }());

    return 0;
}
