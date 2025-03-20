export module frontend.components:selection;

import std;
import glaze;
import web;

import :utils;

namespace frontend::components {

export template<glz::string_literal identifier>
auto selection(std::string_view title,
    const std::unordered_map<std::string, unsigned int>& attributes,
    std::unordered_map<std::string, bool>& selections, unsigned int total = 1, bool show_percent = false)
{
    static frontend::event_context ctx;
    ctx.clear();

    auto search_id = std::format("select_{}_search", identifier.sv());
    auto saved = web::eval("let saved = localStorage.getItem('select_{}_selections'); if(saved === null) {{saved = '{{}}';}}; saved",
        identifier.sv());
    using selection_map = std::unordered_map<std::string, bool>;
    static auto saved_selections = glz::read_json<selection_map>(saved).value_or(selection_map{});
    for(const auto& [s, v] : saved_selections) {
        if(selections.contains(s))
            selections[s] = v;
    }

    using namespace Webxx;
    return fieldset{{_class{"fieldset p-4 rounded-box shadow h-full flex flex-col"}},
        legend{{_class{"fieldset-legend"}}, title},
        label{{_class{"input w-full mb-2 flex flex-col"}},
            ctx.on_input(input{{_id{search_id}, _type{"search"}, _class{"grow basis-[3rem] md:basis-[3.5rem]"}, _placeholder{"Search..."}}},
                [search_id, attributes](std::string_view) {
                    auto search = web::get_property(search_id, "value").value_or("");
                    std::transform(search.begin(), search.end(), search.begin(), [](char c){return std::tolower(c);});

                    for(const auto& [attr, _] : attributes) {
                        std::string name = attr;
                        std::transform(name.begin(), name.end(), name.begin(), [](char c){return std::tolower(c);});

                        auto id = std::format("select_{}_entry_{}", identifier.sv(), attr);
                        if(name.find(search) != std::string::npos) {
                            web::remove_class(id, "hidden");
                        } else {
                            web::add_class(id, "hidden");
                        }
                    }
                })
        },
        dv{{_class{"overflow-y-auto h-full flex flex-col gap-1"}},
            each(attributes, [total, show_percent, &selections](const auto& attr) {
                auto id = std::format("select_{}_entry_{}", identifier.sv(), attr.first);
                auto checkbox_id = std::format("select_{}_entry_{}_checkbox", identifier.sv(), attr.first);

                auto checkbox = ctx.on_change(input{{_id{checkbox_id}, _type{"checkbox"}, _class{"checkbox"}, _ariaLabel{attr.first}, _value{attr.first}}},
                    [checkbox_id, attr, &selections](std::string_view event){
                        bool checked = web::get_property(checkbox_id, "checked") == "true";
                        selections[attr.first] = checked;
                        saved_selections[attr.first] = checked; // maintain those two seperately, so saved_selections can contain values not present in selections
                        web::eval("localStorage.setItem('select_{}_selections', '{}'); ''", identifier.sv(), glz::write_json(saved_selections).value_or("[]"));
                    });
                if(selections[attr.first]) {
                    checkbox.data.attributes.push_back(_checked{});
                }

                return fragment{label{{_id{id}, _class{"fieldset-label"}},
                    std::move(checkbox),
                    show_percent ?
                        std::format("{} ({}%)", attr.first, attr.second*100/total) :
                        std::format("{} ({})", attr.first, attr.second)
                }};
            })
        }
    };
}

}
