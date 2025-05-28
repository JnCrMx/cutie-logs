export module frontend.components:dialog_delete;

import std;
import webxx;
import webpp;
import frontend.assets;

import :alert;
import :utils;

namespace frontend::components {

using success_fn = std::function<void()>;
using error_fn = std::function<void(const std::string&, const std::string&)>;

export Webxx::dialog dialog_delete(event_context& ctx, const std::string& id, std::string what, unsigned int object_id, std::string object_name,
    std::function<void(unsigned int, success_fn success, error_fn error)> delete_callback)
{
    using namespace Webxx;

    return dialog{{_id{id}, _class{"modal"}},
        dv{{_class{"modal-box"}},
            form{{_method{"dialog"}},
                button{{_class{"btn btn-sm btn-circle btn-ghost absolute right-2 top-2"}}, "x"}
            },
            h3{{_class{"text-lg font-bold"}}, std::format("Delete {} \"{}\" ({})", what, object_name, object_id)},
            fieldset{{_class{"fieldset w-full"}},
                components::alert(std::format("{}_error", id)),

                label{{_class{"text-base"}}, std::format("Please type the name of the {} to confirm deletion.", what)},
                ctx.on_input(input{{_id{std::format("{}_name", id)}, _class{"input w-full validator"}, _required{}, _placeholder{"Name"}}},
                    [id, object_name](webpp::event e){
                        auto name = *e.target().as<webpp::element>()->get_property<std::string>("value");
                        bool valid = !name.empty() && name == object_name;
                        if(valid) {
                            webpp::get_element_by_id(std::format("{}_button", id))->remove_class("btn-disabled");
                            webpp::eval("document.getElementById('{}_name').setCustomValidity('');", id);
                        } else {
                            webpp::get_element_by_id(std::format("{}_button", id))->add_class("btn-disabled");
                            webpp::eval("document.getElementById('{}_name').setCustomValidity('Name must match.');", id);
                        }
                    }
                ),
                dv{{_id{"{}_name_validator"}, _class{"validator-hint"}}, "Name must match."},

                ctx.on_click(button{{_id{std::format("{}_button", id)}, _class{"btn btn-error mt-4 w-fit btn-disabled"}},
                    assets::icons::delete_, std::format("Delete {}", what)},
                    [id, object_id, delete_callback](webpp::event e) {
                        delete_callback(object_id, [id]() {
                            components::hide_alert(std::format("{}_error", id));
                            webpp::eval("document.getElementById('{}').close();", id);
                        }, [id](std::string title, std::string message) {
                            components::show_alert(std::format("{}_error", id), title, message);
                        });
                    }
                ),
            }
        },
        form{{_method{"dialog"}, _class{"modal-backdrop"}},
            button{"close"}
        },
    };
}

}
