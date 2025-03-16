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
}

class event_context {
    public:
        auto on_click(auto el, web::event_callback&& cb) {
            auto& data = callbacks.emplace_back(std::make_unique<web::callback_data>(std::move(cb), false));
            el.data.attributes.push_back(Webxx::_dataCallback{std::format("{}", reinterpret_cast<std::uintptr_t>(data.get()))});
            el.data.attributes.push_back(Webxx::_onClick{"handleEvent(this, event);"});
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
    return dv{{_class{"stats shadow grow"}},
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Log Entries"},
            dv{{_class{"stat-value"}}, std::to_string(data.total_logs)},
            dv{{_class{"stat-desc"}}, "Total number of log entries"}
        },
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Attributes"},
            dv{{_class{"stat-value"}}, std::to_string(data.total_attributes)},
            dv{{_class{"stat-desc"}}, "Unique attributes across all logs"}
        },
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Resources"},
            dv{{_class{"stat-value"}}, std::to_string(data.total_resources)},
            dv{{_class{"stat-desc"}}, "Unique resources across all logs"}
        },
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Scopes"},
            dv{{_class{"stat-value"}}, std::to_string(data.total_scopes)},
            dv{{_class{"stat-desc"}}, "Unique scopes across all logs"}
        }
    };
}
auto page_selection(std::string_view what, const std::unordered_map<std::string, int>& attributes) {
    using namespace Webxx;
    int total = attributes.empty() ? 0 : attributes.at("_");
    return fieldset{{_class{"fieldset p-4 rounded-box shadow h-full flex flex-col"}},
        legend{{_class{"fieldset-legend"}}, what},
        label{{_class{"input w-full mb-2"}},
            input{{_type{"search"}, _class{"grow"}, _placeholder{"Search..."}}},
        },
        dv{{_class{"overflow-y-auto h-full flex flex-col gap-1"}},
            each(attributes, [total](const auto& attr) {
                if(attr.first == "_") {
                    return fragment{};
                }
                return fragment{label{{_class{"fieldset-label"}},
                    input{{_type{"checkbox"}, _class{"checkbox"}, _ariaLabel{attr.first}, _value{attr.first}}},
                    attr.first, " (", std::to_string(attr.second*100/total), "%)"
                }};
            })
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
            dv{{_class{"text-1xl font-bold ml-2 grow"}}, "version ", common::project_version},
            dv{{_class{"flex items-center mr-4"}},
                ThemeButton{ctx, current_theme}
            }
        },
        dv{{_class{"w-7xl mx-auto mt-4"}},
            dv{{_id{"stats"}, _class{"flex flex-row justify-center items-center"}}, page_stats({})},
            dv{{_class{"flex flex-row gap-4 h-60"}},
                dv{{_id{"attributes"}, _class{"basis-0 grow"}}, page_selection("Select Attributes", {})},
                dv{{_id{"resources"}, _class{"basis-0 grow"}}, page_selection("Filter Resources", {})},
                dv{{_id{"scopes"}, _class{"basis-0 grow"}}, page_selection("Filter Scopes", {})},
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
        auto att = co_await web::coro::fetch("/api/v1/logs/attributes");
        std::unordered_map<std::string, int> attributes =
            glz::read_json<std::unordered_map<std::string, int>>(co_await web::coro::fetch("/api/v1/logs/attributes"))
            .value_or(std::unordered_map<std::string, int>{});
        web::set_html("attributes", Webxx::render(page_selection("Select Attributes", attributes)));

        std::unordered_map<std::string, int> scopes =
            glz::read_json<std::unordered_map<std::string, int>>(co_await web::coro::fetch("/api/v1/logs/scopes"))
            .value_or(std::unordered_map<std::string, int>{});
        web::set_html("scopes", Webxx::render(page_selection("Filter Scopes", scopes)));

        stats_data stats;
        stats.total_attributes = attributes.size()-1;
        stats.total_logs = attributes["_"];
        stats.total_scopes = scopes.size()-1;
        web::set_html("stats", Webxx::render(page_stats(stats)));

        co_return;
    }());

    return 0;
}
