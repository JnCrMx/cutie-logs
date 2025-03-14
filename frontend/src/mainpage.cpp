import std;
import web;
import webxx;

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
}

class event_context {
    public:
        auto on_event(auto el, std::string_view event, web::event_callback&& cb) {
            auto& data = callbacks.emplace_back(std::make_unique<web::callback_data>(std::move(cb), false));
            el.data.attributes.push_back(Webxx::_dataCallback{std::format("{}", reinterpret_cast<std::uintptr_t>(data.get()))});
            el.data.attributes.push_back(Webxx::internal::HtmlAttribute{std::format("on{}", event).c_str(), {
                "handleEvent(this, event);",
            }});
            return el;
        }
        auto on_click(auto el, web::event_callback&& cb) {
            return on_event(std::move(el), "click", std::move(cb));
        }

        void clear() {
            callbacks.clear();
        }
    private:
        std::vector<std::unique_ptr<web::callback_data>> callbacks;
};

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
                                    _class{"btn btn-block btn-ghost btn-sm justify-start"},
                                    _ariaLabel{theme}, _value{theme}, _checked{}}}, cb)};
                            } else {
                                return li{ctx.on_click(input{{_type{"radio"}, _name{"theme-dropdown"},
                                    _class{"btn btn-block btn-ghost btn-sm justify-start"},
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
        dv{{_class{"flex flex-row items-baseline pt-6 pb-6 sticky top-0 z-10 bg-base-200"}},
            dv{{_class{"text-2xl font-bold ml-4"}}, common::project_name},
            dv{{_class{"grow text-1xl font-bold ml-2"}}, "version ", common::project_version},
            dv{{_class{"flex items-center mr-4"}},
                ThemeButton{ctx, current_theme}
            }
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

    return 0;
}
