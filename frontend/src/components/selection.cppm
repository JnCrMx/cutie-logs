export module frontend.components:selection;

import std;
import glaze;
import web;

import :utils;

namespace frontend::components {

void save_selections(std::string_view identifier, const std::unordered_map<std::string, bool>& saved_selections) {
    web::eval("localStorage.setItem('select_{}_selections', '{}'); ''", identifier, glz::write_json(saved_selections).value_or("[]"));
}

export template<glz::string_literal identifier>
auto selection_detail(std::string_view title,
    const std::unordered_map<std::string, std::tuple<std::string, unsigned int>>& attributes,
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

    std::set<std::tuple<std::string, std::string>> sorted_attributes;
    for(const auto& [key, e] : attributes) {
        sorted_attributes.insert({std::get<0>(e), key});
    }

    auto toggle_id = std::format("select_{}_toggle", identifier.sv());

    using namespace Webxx;
    return fieldset{{_class{"fieldset p-4 rounded-box shadow h-full flex flex-col"}},
        legend{{_class{"fieldset-legend"}}, title},
        dv{{_class{"flex flex-row mb-2 p-0 gap-2 items-center"}},
            ctx.on_change(input{{_id{toggle_id}, _type{"checkbox"}, _class{"checkbox"}}}, [toggle_id, &selections](std::string_view){
                bool checked = web::get_property(toggle_id, "checked") == "true";
                web::log("toggle {}", checked);
                for(auto& [key, value] : selections) {
                    value = checked;
                    saved_selections[key] = checked;
                    web::eval("document.getElementById('select_{}_entry_{}_checkbox').checked = {}; ''", identifier.sv(), key, checked);
                }
                save_selections(identifier.sv(), saved_selections);
            }),
            label{{_class{"input w-full flex flex-col"}},
                ctx.on_input(input{{_id{search_id}, _type{"search"}, _class{"grow basis-[3rem] md:basis-[3.5rem]"}, _placeholder{"Search..."}}},
                    [search_id, attributes](std::string_view) {
                        auto search = web::get_property(search_id, "value").value_or("");
                        std::transform(search.begin(), search.end(), search.begin(), [](char c){return std::tolower(c);});

                        for(const auto& [attr, e] : attributes) {
                            std::string name = std::get<0>(e);
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
        },
        dv{{_class{"overflow-y-auto h-full flex flex-col gap-1"}},
            each(sorted_attributes, [total, show_percent, &attributes, &selections](const auto& attr) {
                auto [name, key] = attr;
                auto count = std::get<1>(attributes.at(key));

                auto id = std::format("select_{}_entry_{}", identifier.sv(), key);
                auto checkbox_id = std::format("select_{}_entry_{}_checkbox", identifier.sv(), key);

                auto checkbox = ctx.on_change(input{{_id{checkbox_id}, _type{"checkbox"}, _class{"checkbox"}, _ariaLabel{name}, _value{name}}},
                    [checkbox_id, key, &selections](std::string_view event){
                        bool checked = web::get_property(checkbox_id, "checked") == "true";
                        selections[key] = checked;
                        saved_selections[key] = checked; // maintain those two seperately, so saved_selections can contain values not present in selections
                        save_selections(identifier.sv(), saved_selections);
                    });
                if(selections[key]) {
                    checkbox.data.attributes.push_back(_checked{});
                }

                return fragment{label{{_id{id}, _class{"fieldset-label"}},
                    std::move(checkbox),
                    show_percent ?
                        std::format("{} ({}%)", name, count*100/total) :
                        std::format("{} ({})", name, count)
                }};
            })
        }
    };
}
export template<glz::string_literal identifier>
auto selection(std::string_view title,
    const std::unordered_map<std::string, unsigned int>& attributes,
    std::unordered_map<std::string, bool>& selections, unsigned int total = 1, bool show_percent = false)
{
    std::unordered_map<std::string, std::tuple<std::string, unsigned int>> attr_map;
    for(const auto& [attr, count] : attributes) {
        attr_map[attr] = {attr, count};
    }
    return selection_detail<identifier>(title, attr_map, selections, total, show_percent);
}

}
