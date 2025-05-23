export module frontend.pages:settings;

import std;
import webpp;
import webxx;
import glaze;

import :page;

import common;
import frontend.assets;
import frontend.components;
import frontend.utils;

namespace frontend::pages {

export class settings : public page {
    private:
        profile_data& profile;
        r<common::log_entry>& example_entry;
        r<common::logs_attributes_response>& attributes;
        r<common::logs_scopes_response>& scopes;
        r<common::logs_resources_response>& resources;
        common::advanced_stencil_functions stencil_functions;

        common::cleanup_rules_response cleanup_rules;
    public:
        settings(profile_data& profile, r<common::log_entry>& example_entry, r<common::logs_attributes_response>& attributes, r<common::logs_scopes_response>& scopes, r<common::logs_resources_response>& resources,
            std::vector<std::pair<std::string_view, common::mmdb*>> mmdbs)
            : profile(profile), example_entry{example_entry}, attributes{attributes}, scopes{scopes}, resources{resources}, stencil_functions{std::move(mmdbs)}
        {
            profile.add_callback([this](auto&) { if(is_open) { open(); }});
        }

        webpp::coroutine<void> refresh() {
            auto data = co_await webpp::coro::fetch("/api/v1/settings/cleanup_rules", utils::fetch_options)
                .then(std::mem_fn(&webpp::response::co_bytes));
            cleanup_rules = glz::read_beve<common::cleanup_rules_response>(data).value_or(common::cleanup_rules_response{});

            webpp::get_element_by_id("settings_cleanup_rules")->inner_html(Webxx::render(render_cleanup_rules()));
        }

        void open() override {
            page::open();

            webpp::coro::submit(refresh());
        }

        Webxx::dialog render_cleanup_rule_dialog(event_context& ctx, std::optional<common::cleanup_rule> rule = std::nullopt) {
            using namespace Webxx;
            bool edit = rule.has_value();
            bool enabled = edit ? rule->enabled : false;

            return dialog{{_id{"dialog_add_cleanup_rule"}, _class{"modal"}},
                dv{{_class{"modal-box w-3xl max-w-3xl"}},
                    form{{_method{"dialog"}},
                        button{{_class{"btn btn-sm btn-circle btn-ghost absolute right-2 top-2"}}, "x"}
                    },
                    h3{{_class{"text-lg font-bold"}}, edit ? std::format("Edit cleanup rule \"{}\" ({})", rule->name, rule->id) : "Add cleanup rule"},
                    fieldset{{_class{"fieldset w-full"}},
                        components::alert("dialog_add_cleanup_rule_error"),

                        label{{_class{"fieldset-label"}}, "Rule name"},
                        ctx.on_input(input{{_id{"dialog_add_cleanup_rule_name"}, _class{"input w-full validator"}, _required{}, _placeholder{"Name"}, _value{edit ? rule->name : ""}}},
                            [this](webpp::event e){
                                auto name = *e.target().as<webpp::element>()->get_property<std::string>("value");
                                bool valid = !name.empty();
                                if(valid) {
                                    webpp::get_element_by_id("dialog_add_cleanup_rule_button")->remove_class("btn-disabled");
                                    webpp::eval("document.getElementById('dialog_add_cleanup_rule_name').setCustomValidity('');");
                                } else {
                                    webpp::get_element_by_id("dialog_add_cleanup_rule_button")->add_class("btn-disabled");
                                    webpp::eval("document.getElementById('dialog_add_cleanup_rule_name').setCustomValidity('Rule name must be non-empty and unique.');");
                                }
                            }
                        ),
                        dv{{_id{"dialog_add_cleanup_rule_name_validator"}, _class{"validator-hint"}}, "Rule name must be non-empty and unique."},

                        label{{_class{"fieldset-label"}}, "Rule description"},
                        textarea{{_id{"dialog_add_cleanup_rule_description"}, _class{"textarea w-full"}, _placeholder{"Description"}}, edit ? rule->description : ""},
                        dv{{_class{"validator-hint invisible"}}, "Just here for equal spacing."},

                        dv{{_class{"flex flex-row gap-16 w-full"}},
                            dv{{_class{"flex flex-col"}},
                                label{{_class{"fieldset-label"}}, "Enabled"},
                                dv{{_class{"flex grow items-center"}},
                                    enabled ?
                                        input{{_id{"dialog_add_cleanup_rule_enabled"}, _type{"checkbox"}, _class{"toggle toggle-xl toggle-primary"}, _checked{}}} :
                                        input{{_id{"dialog_add_cleanup_rule_enabled"}, _type{"checkbox"}, _class{"toggle toggle-xl toggle-primary"}}}
                                },
                                dv{{_class{"validator-hint invisible"}}, "Just here for equal spacing."},
                            },
                            dv{{_class{"grow"}},
                                label{{_class{"fieldset-label"}}, "Execution interval"},
                                components::duration_picker("dialog_add_cleanup_rule_execution_interval", "Execution interval",
                                    dv{{_class{"validator-hint w-[200%]"}}, "Execution interval must be a positive number."},
                                    rule.transform([](const auto& v){return v.execution_interval;})
                                )
                            }
                        },

                        fieldset{{_class{"fieldset p-4 rounded-box shadow w-full h-full flex flex-col"}},
                            legend{{_class{"fieldset-legend"}}, "Conditions"},

                            label{{_class{"fieldset-label"}}, "Minimum age"},
                            components::duration_picker("dialog_add_cleanup_rule_minimum_age", "Minimum age",
                                dv{{_class{"validator-hint w-[200%]"}}, "Minimum age must be a positive number."},
                                rule.transform([](const auto& v){return v.filter_minimum_age;})
                            )
                        },

                        ctx.on_click(button{{_id{"dialog_add_cleanup_rule_button"}, _class{edit ? "btn btn-warning mt-4 w-fit" : "btn btn-success mt-4 w-fit btn-disabled"}},
                            assets::icons::add, edit ? "Save rule" : "Add rule"},
                            [this, edit, rule](webpp::event e) {
                                webpp::coro::submit([this](bool edit, std::optional<common::cleanup_rule> rule) -> webpp::coroutine<void> {
                                    common::cleanup_rule new_rule = rule.value_or(common::cleanup_rule{});
                                    new_rule.name = *webpp::get_element_by_id("dialog_add_cleanup_rule_name")->get_property<std::string>("value");
                                    new_rule.description = webpp::get_element_by_id("dialog_add_cleanup_rule_description")->get_property<std::string>("value").value_or("");
                                    new_rule.enabled = webpp::get_element_by_id("dialog_add_cleanup_rule_enabled")->get_property<bool>("checked").value_or(false);
                                    new_rule.execution_interval = components::get_duration("dialog_add_cleanup_rule_execution_interval");

                                    new_rule.filter_minimum_age = components::get_duration("dialog_add_cleanup_rule_minimum_age");

                                    std::string beve = glz::write_beve(new_rule).value_or("null");

                                    webpp::js_object request = webpp::js_object::create();
                                    request["headers"] = utils::fetch_headers;
                                    request["body"] = webpp::uint8array::create(beve);

                                    webpp::response res;
                                    if(edit) {
                                        request["method"] = "PATCH";
                                        res = co_await webpp::coro::fetch(std::format("/api/v1/settings/cleanup_rules/{}", rule->id), request);
                                    } else {
                                        request["method"] = "PUT";
                                        res = co_await webpp::coro::fetch("/api/v1/settings/cleanup_rules", request);
                                    }

                                    if(!res.ok()) {
                                        std::string message = co_await res.co_text();
                                        components::show_alert("dialog_add_cleanup_rule_error", res.status_text(), message);
                                        co_return;
                                    }
                                    auto new_rule_expected = glz::read_beve<common::cleanup_rule>(co_await res.co_bytes());
                                    if(!new_rule_expected) {
                                        components::show_alert("dialog_add_cleanup_rule_error", "Failed to parse rule", glz::format_error(new_rule_expected.error()));
                                        co_return;
                                    }

                                    webpp::eval("document.getElementById('dialog_add_cleanup_rule').close();");

                                    cleanup_rules.rules[new_rule_expected->id] = *new_rule_expected;
                                    webpp::get_element_by_id("settings_cleanup_rules")->inner_html(Webxx::render(render_cleanup_rules()));

                                    co_return;
                                }(edit, rule));
                            }
                        ),
                    }
                },
                form{{_method{"dialog"}, _class{"modal-backdrop"}},
                    button{"close"}
                },
            };
        }

        Webxx::dialog render_cleanup_rule_delete_dialog(event_context& ctx, common::cleanup_rule rule) {
            using namespace Webxx;

            return dialog{{_id{"dialog_delete_cleanup_rule"}, _class{"modal"}},
                dv{{_class{"modal-box"}},
                    form{{_method{"dialog"}},
                        button{{_class{"btn btn-sm btn-circle btn-ghost absolute right-2 top-2"}}, "x"}
                    },
                    h3{{_id{"dialog_delete_cleanup_rule_title"}, _class{"text-lg font-bold"}}, std::format("Delete cleanup rule \"{}\" ({})", rule.name, rule.id)},
                    fieldset{{_class{"fieldset w-full"}},
                        components::alert("dialog_delete_cleanup_rule_error"),

                        label{{_class{"text-base"}}, "Please type the name of the rule to confirm deletion."},
                        ctx.on_input(input{{_id{"dialog_delete_cleanup_rule_name"}, _class{"input w-full validator"}, _required{}, _placeholder{"Name"}}},
                            [rule](webpp::event e){
                                auto name = *e.target().as<webpp::element>()->get_property<std::string>("value");
                                bool valid = !name.empty() && name == rule.name;
                                if(valid) {
                                    webpp::get_element_by_id("dialog_delete_cleanup_rule_button")->remove_class("btn-disabled");
                                    webpp::eval("document.getElementById('dialog_delete_cleanup_rule_name').setCustomValidity('');");
                                } else {
                                    webpp::get_element_by_id("dialog_delete_cleanup_rule_button")->add_class("btn-disabled");
                                    webpp::eval("document.getElementById('dialog_delete_cleanup_rule_name').setCustomValidity('Rule name must match.');");
                                }
                            }
                        ),
                        dv{{_id{"dialog_delete_cleanup_rule_name_validator"}, _class{"validator-hint"}}, "Rule name must match."},

                        ctx.on_click(button{{_id{"dialog_delete_cleanup_rule_button"}, _class{"btn btn-error mt-4 w-fit btn-disabled"}},
                            assets::icons::delete_, "Delete rule"},
                            [this, id = rule.id](webpp::event e) {
                                webpp::coro::submit([this](unsigned int id) -> webpp::coroutine<void> {
                                    webpp::js_object request = webpp::js_object::create();
                                    request["headers"] = utils::fetch_headers;
                                    request["method"] = "DELETE";
                                    webpp::response res = co_await webpp::coro::fetch(std::format("/api/v1/settings/cleanup_rules/{}", id), request);
                                    if(!res.ok()) {
                                        std::string message = co_await res.co_text();
                                        components::show_alert("dialog_delete_cleanup_rule_error", res.status_text(), message);
                                        co_return;
                                    }

                                    webpp::eval("document.getElementById('dialog_delete_cleanup_rule').close();");

                                    cleanup_rules.rules.erase(id);
                                    webpp::get_element_by_id("settings_cleanup_rules")->inner_html(Webxx::render(render_cleanup_rules()));

                                    co_return;
                                }(id));
                            }
                        ),
                    }
                },
                form{{_method{"dialog"}, _class{"modal-backdrop"}},
                    button{"close"}
                },
            };
        }

        using js_handle = decltype(std::declval<webpp::js_object>().handle());
        std::unique_ptr<webpp::callback_data> cb_edit_cleanup_rule = std::make_unique<webpp::callback_data>(
            [this](js_handle h, std::string_view) {
                webpp::event event{h};
                webpp::element target = *event.current_target().as<webpp::element>();

                unsigned int id = std::stoi(*target["dataset"]["ruleId"].as<std::string>());
                const auto& rule = cleanup_rules.rules[id];

                static event_context ctx;
                ctx.clear();

                webpp::get_element_by_id("dialog_placeholder")->inner_html(Webxx::render(render_cleanup_rule_dialog(ctx, rule)));
                webpp::eval("document.getElementById('dialog_add_cleanup_rule').showModal();");
            }, false
        );
        std::unique_ptr<webpp::callback_data> cb_delete_cleanup_rule = std::make_unique<webpp::callback_data>(
            [this](js_handle h, std::string_view) {
                webpp::event event{h};
                webpp::element target = *event.current_target().as<webpp::element>();

                unsigned int id = std::stoi(*target["dataset"]["ruleId"].as<std::string>());
                const auto& rule = cleanup_rules.rules[id];

                static event_context ctx;
                ctx.clear();

                webpp::get_element_by_id("dialog_placeholder")->inner_html(Webxx::render(render_cleanup_rule_delete_dialog(ctx, rule)));
                webpp::eval("document.getElementById('dialog_delete_cleanup_rule').showModal();");
            }, false
        );
        std::unique_ptr<webpp::callback_data> cb_toggle_cleanup_rule = std::make_unique<webpp::callback_data>(
            [this](js_handle h, std::string_view) {
                webpp::coro::submit([this](webpp::event event) -> webpp::coroutine<void> {
                    webpp::element target = *event.current_target().as<webpp::element>();

                    unsigned int id = std::stoi(*target["dataset"]["ruleId"].as<std::string>());
                    auto& rule = cleanup_rules.rules[id];
                    rule.enabled = target.get_property<bool>("checked").value_or(false);

                    std::string beve = glz::write_beve(rule).value_or("null");

                    webpp::js_object request = webpp::js_object::create();
                    request["headers"] = utils::fetch_headers;
                    request["method"] = "PATCH";
                    request["body"] = webpp::uint8array::create(beve);

                    webpp::response res = co_await webpp::coro::fetch(std::format("/api/v1/settings/cleanup_rules/{}", rule.id), request);
                    if(!res.ok()) {
                        std::string message = co_await res.co_text();
                        components::show_alert("settings_cleanup_rules_error", res.status_text(), message);
                        co_return;
                    }
                    auto new_rule_expected = glz::read_beve<common::cleanup_rule>(co_await res.co_bytes());
                    if(!new_rule_expected) {
                        components::show_alert("settings_cleanup_rules_error", "Failed to parse rule", glz::format_error(new_rule_expected.error()));
                        co_return;
                    }

                    cleanup_rules.rules[new_rule_expected->id] = *new_rule_expected;
                    webpp::get_element_by_id("settings_cleanup_rules")->inner_html(Webxx::render(render_cleanup_rules()));

                    co_return;
                }(webpp::event{h}));
            }, false
        );

        auto render_cleanup_rule(const common::cleanup_rule& rule) {
            using namespace Webxx;
            using std::string_view_literals::operator""sv;

            constexpr static char dataRuleIdAttr[] = "data-rule-id";
            using _dataRuleId = Webxx::attr<dataRuleIdAttr>;

            auto checkbox = rule.enabled ?
                input{{_type{"checkbox"}, _class{"toggle toggle-primary"}, _checked{}}} :
                input{{_type{"checkbox"}, _class{"toggle toggle-primary"}}};
            checkbox.data.attributes.emplace_back(_dataRuleId{std::to_string(rule.id)});
            checkbox = events::on_change(std::move(checkbox), cb_toggle_cleanup_rule.get());

            std::string run_info = rule.last_execution ?
                std::format("Last run: {}", *rule.last_execution) :
                "Never run";
            std::string generated_decription = "Delete logs older than " + common::format_duration(rule.filter_minimum_age);
            auto add_filter = [&](std::string_view name_singular, std::string_view name_plural, std::string_view yes_verb, std::string_view no_verb, const auto& v) {
                if(v.values.empty() && v.type != common::filter_type::INCLUDE) {
                    return;
                }
                if(v.values.empty()) {
                    generated_decription += "<span class=\"text-error\">";
                }
                generated_decription += std::format(" {} {} {}{:n}{}",
                    v.type == common::filter_type::INCLUDE ? yes_verb : no_verb,
                    v.values.size() == 1 ? name_singular : name_plural,
                    v.values.size() == 1 ? ""sv : "["sv,
                    v.values,
                    v.values.size() == 1 ? ""sv : "]"sv
                );
                if(v.values.empty()) {
                    generated_decription += "</span>";
                }
                generated_decription += " and";
            };

            add_filter("resource"sv, "resources"sv, "having"sv, "not having"sv, rule.filters.resources);
            add_filter("scope"sv, "scopes"sv, "having"sv, "not having"sv, rule.filters.scopes);
            add_filter("severity"sv, "severities"sv, "being of"sv, "not being of"sv, rule.filters.severities);
            add_filter("attribute"sv, "attributes"sv, "having"sv, "not having"sv, rule.filters.attributes);
            if(!rule.filters.attribute_values.values.empty()) {
                generated_decription += std::format(" {} attributes {}",
                    rule.filters.attribute_values.type == common::filter_type::INCLUDE ? "having"sv : "not having"sv,
                    rule.filters.attribute_values.values.dump().value_or("null")
                );
            }
            if(generated_decription.ends_with(" and")) {
                generated_decription = generated_decription.substr(0, generated_decription.size() - 4);
            }
            generated_decription += ".";

            return li{{_class{"list-row items-center"}},
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Enable/Disable rule"}}, std::move(checkbox)},
                dv{{_class{"list-col-grow flex flex-row items-center gap-4"}},
                    h3{{_class{"text-lg font-bold"}}, rule.name},
                    p{{_class{"text-sm text-base-content/80"}}, std::format("Execution interval: {}", common::format_duration(rule.execution_interval))},
                },
                dv{{_class{"text-sm text-base-content/80 list-col-wrap"}},
                    p{rule.description.empty() ? fragment{i{"No description"}} : fragment{b{rule.description}}},
                    p{generated_decription}
                },
                p{{_class{"text-sm text-base-content/80"}}, run_info},
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Edit rule"}},
                    events::on_click(button{{_class{"btn btn-secondary btn-square"}, _dataRuleId{std::to_string(rule.id)}}, assets::icons::edit}, cb_edit_cleanup_rule.get()),
                },
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Delete rule"}},
                    events::on_click(button{{_class{"btn btn-secondary btn-square"}, _dataRuleId{std::to_string(rule.id)}}, assets::icons::delete_}, cb_delete_cleanup_rule.get()),
                },
            };
        }

        Webxx::fragment render_cleanup_rules() {
            static event_context ctx;
            ctx.clear();

            using namespace Webxx;
            return fragment{
                components::alert("settings_cleanup_rules_error", "mb-4"),
                dv{{_class{"flex flex-col gap-4 w-fit mb-4"}},
                    ctx.on_click(button{{_class{"btn btn-primary"}}, assets::icons::add, "Add cleanup rule"},
                        [this](webpp::event e) {
                            static event_context sctx;
                            sctx.clear();

                            webpp::get_element_by_id("dialog_placeholder")->inner_html(Webxx::render(render_cleanup_rule_dialog(sctx)));
                            webpp::eval("document.getElementById('dialog_add_cleanup_rule').showModal();");
                        })
                },
                ul{{_class{"list bg-base-200 rounded-box shadow-md"}},
                    each(cleanup_rules.rules, [&](const auto& rule) {
                        return render_cleanup_rule(rule.second);
                    }),
                    maybe(cleanup_rules.rules.empty(), [&]() {
                        return li{{_class{"list-row items-center"}},
                            h3{{_class{"text-lg font-bold text-base-content/80"}}, "No cleanup rules found"},
                            p{{_class{"text-sm text-base-content/80 list-col-grow"}}, "You can add a new rule by clicking the button above."}
                        };
                    })
                },
            };
        }

        Webxx::fragment render() override {
            static event_context ctx;
            ctx.clear();

            using namespace Webxx;
            return Webxx::fragment {
                dv{{_class{"tabs tabs-border tabs-lg"}},
                    input{{_type{"radio"}, _name{"settings_cleanup_rules"}, _class{"tab"}, _ariaLabel{"Cleanup rules"}, _checked{}}},
                    dv{{_id{"settings_cleanup_rules"}, _class{"tab-content border-base-300 bg-base-100 p-10"}}, render_cleanup_rules()}
                },
                dv{{_id{"dialog_placeholder"}}},
            };
        }
};

}
