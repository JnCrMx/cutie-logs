export module frontend.components:selection;

import std;
import glaze;
import webpp;
import i18n;

import :utils;

using namespace mfk::i18n::literals;

namespace frontend::components {

void save_selections(profile_data* profile, std::string_view identifier, const std::unordered_map<std::string, bool>& saved_selections) {
    if(!profile) {
        return;
    }
    profile->set_data(std::format("select_{}_selections", identifier), glz::write_json(saved_selections).value_or("{}"));
}

export template<glz::string_literal identifier>
auto selection_detail(std::string_view title,
    const std::unordered_map<std::string, std::tuple<std::string, unsigned int>>& attributes, std::unordered_map<std::string, bool>& selections,
    profile_data* profile, unsigned int total = 1, bool show_percent = false, std::string_view info_modal_key = "")
{
    static frontend::event_context ctx;
    ctx.clear();

    auto search_id = std::format("select_{}_search", identifier.sv());
    std::string saved = profile ? profile->get_data(std::format("select_{}_selections", identifier.sv())).value_or("{}") : "{}";
    using selection_map = std::unordered_map<std::string, bool>;
    static selection_map saved_selections;

    saved_selections = glz::read_json<selection_map>(saved).value_or(selection_map{});
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
            ctx.on_change(input{{_id{toggle_id}, _type{"checkbox"}, _class{"checkbox"}}}, [toggle_id, &selections, profile](webpp::event){
                bool checked = webpp::get_element_by_id(toggle_id)->template get_property<bool>("checked").value_or(false);
                for(auto& [key, value] : selections) {
                    value = checked;
                    saved_selections[key] = checked;
                    webpp::get_element_by_id(std::format("select_{}_entry_{}_checkbox", identifier.sv(), key))->set_property("checked", checked);
                }
                save_selections(profile, identifier.sv(), saved_selections);
            }),
            label{{_class{"input w-full flex flex-col"}},
                ctx.on_input(input{{_id{search_id}, _type{"search"}, _class{"grow basis-[3rem] md:basis-[3.5rem]"}, _placeholder{"Search..."_}}},
                    [search_id, attributes](webpp::event) {
                        auto search = webpp::get_element_by_id(search_id)->template get_property<std::string>("value").value_or("");
                        std::transform(search.begin(), search.end(), search.begin(), [](char c){return std::tolower(c);});

                        for(const auto& [attr, e] : attributes) {
                            std::string name = std::get<0>(e);
                            std::transform(name.begin(), name.end(), name.begin(), [](char c){return std::tolower(c);});

                            auto id = std::format("select_{}_entry_{}", identifier.sv(), attr);
                            if(name.find(search) != std::string::npos) {
                                webpp::get_element_by_id(id)->remove_class("hidden");
                            } else {
                                webpp::get_element_by_id(id)->add_class("hidden");
                            }
                        }
                    })
            },
        },
        dv{{_class{"overflow-y-auto h-full flex flex-col gap-1"}},
            each(sorted_attributes, [total, show_percent, &attributes, &selections, info_modal_key, profile](const auto& attr) {
                auto [name, key] = attr;
                auto count = std::get<1>(attributes.at(key));

                auto id = std::format("select_{}_entry_{}", identifier.sv(), key);
                auto checkbox_id = std::format("select_{}_entry_{}_checkbox", identifier.sv(), key);

                auto checkbox = ctx.on_change(input{{_id{checkbox_id}, _type{"checkbox"}, _class{"checkbox"}, _ariaLabel{name}, _value{name}}},
                    [checkbox_id, key, &selections, profile](webpp::event){
                        bool checked = webpp::get_element_by_id(checkbox_id)->template get_property<bool>("checked").value_or(false);
                        selections[key] = checked;
                        saved_selections[key] = checked; // maintain those two seperately, so saved_selections can contain values not present in selections
                        save_selections(profile, identifier.sv(), saved_selections);
                    });
                if(selections[key]) {
                    checkbox.data.attributes.push_back(_checked{});
                }

                return fragment{label{{_id{id}, _class{"fieldset-label flex flex-row w-full"}},
                    std::move(checkbox),
                    span{{_class{"grow"}},show_percent ?
                        std::format("{} ({}%)", sanitize(name), count*100/total) :
                        std::format("{} ({})", sanitize(name), count)},
                    maybe(!info_modal_key.empty(), [key, info_modal_key]() {
                        return button{{_class{"btn btn-xs btn-outline btn-circle mr-2"},
                            _onClick{std::format("document.getElementById('modal_{}_{}').showModal()", info_modal_key, key)}},
                            "i"
                        };
                    })
                }};
            })
        }
    };
}
export template<glz::string_literal identifier>
auto selection(std::string_view title,
    const std::unordered_map<std::string, unsigned int>& attributes, std::unordered_map<std::string, bool>& selections,
    profile_data* profile, unsigned int total = 1, bool show_percent = false)
{
    std::unordered_map<std::string, std::tuple<std::string, unsigned int>> attr_map;
    for(const auto& [attr, count] : attributes) {
        attr_map[attr] = {attr, count};
    }
    return selection_detail<identifier>(title, attr_map, selections, profile, total, show_percent);
}

}
