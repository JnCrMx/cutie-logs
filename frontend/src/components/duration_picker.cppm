export module frontend.components:duration_picker;

import std;
import webxx;

import :utils;
import common;

namespace frontend::components {

export auto duration_picker(std::string_view id, std::string_view placeholder, auto validator, std::optional<std::chrono::seconds> duration = std::nullopt) {
    using namespace Webxx;

    constexpr std::array indices = {0, 1, 2, 3};
    constexpr std::array units = {"seconds", "minutes", "hours", "days"};
    constexpr std::array unit_values = {1, 60, 3600, 86400};
    unsigned int selected_unit = std::numeric_limits<unsigned int>::max();
    double value = 0;

    if(duration) {
        for(unsigned int i=0; i<units.size(); i++) {
            if(duration->count() >= unit_values[i]) {
                selected_unit = i;
                value = static_cast<double>(duration->count()) / unit_values[i];
            }
        }
    }

    return dv{{_id{id}, _class{"flex flex-row gap-4 w-full"}},
        dv{{_class{"grow"}},
            (duration ?
                input{{_id{std::format("{}_input", id)}, _type{"number"}, _class{"input validator"}, _min{"1"}, _step{"any"}, _placeholder{placeholder}, _value{std::format("{:.2f}", value)}}} :
                input{{_id{std::format("{}_input", id)}, _type{"number"}, _class{"input validator"}, _min{"1"}, _step{"any"}, _placeholder{placeholder}}}
            ),
            validator,
        },
        select{{_id{std::format("{}_unit", id)}, _class{"select grow"}},
            each(std::views::zip(indices, units, unit_values), [&](auto e) {
                auto [index, unit, unit_value] = e;
                if(index == selected_unit) {
                    return option{{_value{std::to_string(unit_value)}, _selected{}}, unit};
                } else {
                    return option{{_value{std::to_string(unit_value)}}, unit};
                }
            })
        }
    };
}

export std::chrono::seconds get_duration(std::string_view id) {
    auto input = webpp::get_element_by_id(std::format("{}_input", id));
    auto select = webpp::get_element_by_id(std::format("{}_unit", id));

    if(!input || !select) {
        return std::chrono::seconds{0};
    }
    auto value = common::parse_double(input->get_property<std::string>("value").value_or("0")).value_or(0);
    auto unit = common::parse_int(select->get_property<std::string>("value").value_or("0")).value_or(0);
    return std::chrono::duration_cast<std::chrono::seconds>(value * unit * std::chrono::seconds{1});
}

}
