export module frontend.components:theme_button;

import std;
import webxx;

import frontend.assets;

import :utils;

namespace frontend::components {

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
export struct theme_button : component<theme_button> {
    theme_button(event_context& ctx, std::string_view current) : component<theme_button>{
        dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Themes"}},
            dv{{_class{"dropdown dropdown-end dropdown-bottom"}},
                dv{{_class{"btn btn-square m-1"}, _tabindex{"0"}, _role{"button"}}, assets::icons::themes},
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

}
