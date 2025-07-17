export module frontend.components:dialog_add;

import std;
import webxx;
import webpp;
import i18n;
import frontend.assets;

import :alert;
import :utils;

using namespace mfk::i18n::literals;

namespace frontend::components {

using success_fn = std::function<void()>;
using error_fn = std::function<void(const std::string&, const std::string&)>;
using valid_fn = std::function<void(bool)>;

export struct dialog_add_ctx {
    std::string what;
    bool edit;

    unsigned int object_id = 0;
    std::string object_name = "";
    std::string object_description = "";

    std::function<std::optional<std::string>(dialog_add_ctx&, const std::string&)> name_validator;
    std::function<Webxx::fragment(dialog_add_ctx&, valid_fn)> content;
    std::function<void(dialog_add_ctx&, unsigned int id, const std::string& name, const std::string& description, success_fn, error_fn)> add_callback;

    bool name_valid = false;
    bool user_valid = true;
    bool valid() const {
        return name_valid && user_valid;
    }

    std::any user_data;
};

export Webxx::dialog dialog_add(event_context& ctx, const std::string& id, dialog_add_ctx&& args)
{
    using namespace Webxx;
    auto params = std::make_shared<dialog_add_ctx>(std::move(args));

    if(params->edit) {
        params->name_valid = !params->name_validator(*params, params->object_name).has_value();
    }

    return dialog{{_id{id}, _class{"modal"}},
        dv{{_class{"modal-box w-3xl max-w-3xl"}},
            form{{_method{"dialog"}},
                button{{_class{"btn btn-sm btn-circle btn-ghost absolute right-2 top-2"}}, "x"}
            },
            h3{{_class{"text-lg font-bold"}}, params->edit ?
                // note for translator: the scheme is "Edit [what] "[name]" ([id])"; example: "Edit cleanup rule "My Rule" (123)"
                "Edit {} \"{}\" ({})"_(params->what, params->object_name, params->object_id) :
                // note for translator: the scheme is "Add [what]"; example: "Add cleanup rule"
                "Add {}"_(params->what)
            },
            fieldset{{_class{"fieldset w-full"}},
                components::alert(std::format("{}_error", id)),

                label{{_class{"fieldset-label"}}, "{} name"_(params->what)},
                ctx.on_input(input{{_id{std::format("{}_name", id)}, _class{"input w-full validator"}, _required{}, _placeholder{"Name"_}, _value{params->object_name}}},
                    [id, params](webpp::event e){
                        auto name = *e.target().as<webpp::element>()->get_property<std::string>("value");
                        auto error = params->name_validator(*params, name);
                        if(!error) {
                            webpp::eval("document.getElementById('{}_name').setCustomValidity('');", id);
                        } else {
                            webpp::get_element_by_id(std::format("{}_name_validator", id))->inner_text(*error);
                            webpp::eval("document.getElementById('{}_name').setCustomValidity({});", id, glz::write_json(*error).value_or("\"error\""));
                        }
                        params->name_valid = !error.has_value();
                        if(params->valid()) {
                            webpp::get_element_by_id(std::format("{}_button", id))->remove_class("btn-disabled");
                        } else {
                            webpp::get_element_by_id(std::format("{}_button", id))->add_class("btn-disabled");
                        }
                    }
                ),
                dv{{_id{std::format("{}_name_validator", id)}, _class{"validator-hint"}}, "Name must be valid."_},

                // note for translator: the scheme is "[what] description"; example: "Cleanup rule description"
                label{{_class{"fieldset-label"}}, "{} description"_(params->what)},
                textarea{{_id{std::format("{}_description", id)}, _class{"textarea w-full"}, _placeholder{"Description"_}}, params->object_description},
                dv{{_class{"validator-hint invisible"}}, "&nbsp;"},

                params->content(*params, [params, id](bool valid) {
                    params->user_valid = valid;
                    if(params->valid()) {
                        webpp::get_element_by_id(std::format("{}_button", id))->remove_class("btn-disabled");
                    } else {
                        webpp::get_element_by_id(std::format("{}_button", id))->add_class("btn-disabled");
                    }
                }),

                ctx.on_click(button{{_id{std::format("{}_button", id)}, _class{params->edit ? "btn btn-warning mt-4 w-fit" : "btn btn-success mt-4 w-fit btn-disabled"}},
                    assets::icons::add, params->edit ?
                        // note for translator: the scheme is "Save [what]"; example: "Save cleanup rule"
                        "Save {}"_(params->what) :
                        // note for translator: the scheme is "Add [what]"; example: "Add cleanup rule"
                        "Add {}"_(params->what)},
                    [id, params](webpp::event e) {
                        e.prevent_default();
                        auto name = *webpp::get_element_by_id(std::format("{}_name", id))->get_property<std::string>("value");
                        auto description = *webpp::get_element_by_id(std::format("{}_description", id))->get_property<std::string>("value");

                        params->add_callback(*params, params->object_id, name, description,
                            [id]() {
                                components::hide_alert(std::format("{}_error", id));
                                webpp::eval("document.getElementById('{}').close();", id);
                            },
                            [id](std::string title, std::string message) {
                                components::show_alert(std::format("{}_error", id), title, message);
                            }
                        );
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
