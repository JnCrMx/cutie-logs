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
                        label{{_class{"fieldset-label"}}, "Rule name"},
                        ctx.on_input(input{{_id{"rule_name"}, _class{"input w-full validator"}, _required{}, _placeholder{"Name"}, _value{edit ? rule->name : ""}}},
                            [this](webpp::event e){
                                auto name = *e.target().as<webpp::element>()->get_property<std::string>("value");
                                bool valid = !name.empty();
                                if(valid) {
                                    webpp::get_element_by_id("cleanup_rule_add")->remove_class("btn-disabled");
                                    webpp::eval("document.getElementById('rule_name').setCustomValidity('');");
                                } else {
                                    webpp::get_element_by_id("cleanup_rule_add")->add_class("btn-disabled");
                                    webpp::eval("document.getElementById('rule_name').setCustomValidity('Rule name must be non-empty and unique.');");
                                }
                            }
                        ),
                        dv{{_id{"rule_name_validator"}, _class{"validator-hint"}}, "Rule name must be non-empty and unique."},

                        label{{_class{"fieldset-label"}}, "Rule description"},
                        textarea{{_id{"rule_description"}, _class{"textarea w-full"}, _placeholder{"Description"}}, edit ? rule->description : ""},
                        dv{{_class{"validator-hint invisible"}}, "Just here for equal spacing."},

                        dv{{_class{"flex flex-row gap-16 w-full"}},
                            dv{{_class{"flex flex-col"}},
                                label{{_class{"fieldset-label"}}, "Enabled"},
                                dv{{_class{"flex grow items-center"}},
                                    enabled ?
                                        input{{_id{"rule_enabled"}, _type{"checkbox"}, _class{"toggle toggle-xl toggle-primary"}, _checked{}}} :
                                        input{{_id{"rule_enabled"}, _type{"checkbox"}, _class{"toggle toggle-xl toggle-primary"}}}
                                },
                                dv{{_class{"validator-hint invisible"}}, "Just here for equal spacing."},
                            },
                            dv{{_class{"grow"}},
                                label{{_class{"fieldset-label"}}, "Execution interval"},
                                components::duration_picker("rule_execution_interval", "Execution interval",
                                    dv{{_class{"validator-hint w-[200%]"}}, "Execution interval must be a positive number."},
                                    rule.transform([](const auto& v){return v.execution_interval;})
                                )
                            }
                        },

                        fieldset{{_class{"fieldset p-4 rounded-box shadow w-full h-full flex flex-col"}},
                            legend{{_class{"fieldset-legend"}}, "Conditions"},

                            label{{_class{"fieldset-label"}}, "Minimum age"},
                            components::duration_picker("rule_minimum_age", "Minimum age",
                                dv{{_class{"validator-hint w-[200%]"}}, "Minimum age must be a positive number."},
                                rule.transform([](const auto& v){return v.filter_minimum_age;})
                            )
                        },

                        ctx.on_click(button{{_id{"cleanup_rule_add"}, _class{"btn btn-neutral mt-4 w-fit btn-disabled"}},
                            assets::icons::add, edit ? "Save rule" : "Add rule"},
                            [this](webpp::event e) {
                                // TODO
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
            [](js_handle h, std::string_view) {
                webpp::event event{h};
                webpp::element target = *event.current_target().as<webpp::element>();

                unsigned int id = std::stoi(*target["dataset"]["ruleId"].as<std::string>());

                webpp::log("Delete cleanup rule clicked for rule id: {}", id);
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

            std::string run_info = rule.last_execution ?
                std::format("Last run: {}", *rule.last_execution) :
                "Never run";
            std::string generated_decription = "Delete logs older than " + utils::format_duration(rule.filter_minimum_age);
            if(!rule.filter_resources.values.empty()) {
                generated_decription += std::format(" {} {} {}{:n}{} and",
                    rule.filter_resources.type == common::filter_type::INCLUDE ? "belonging to"sv : "not belonging to"sv,
                    rule.filter_resources.values.size() == 1 ? "resource"sv : "resources"sv,
                    rule.filter_resources.values.size() == 1 ? ""sv : "["sv,
                    rule.filter_resources.values,
                    rule.filter_resources.values.size() == 1 ? ""sv : "]"sv
                );
            }
            if(!rule.filter_scopes.values.empty()) {
                generated_decription += std::format(" {} {} {}{:n}{} and",
                    rule.filter_scopes.type == common::filter_type::INCLUDE ? "having"sv : "not having"sv,
                    rule.filter_scopes.values.size() == 1 ? "scope"sv : "scopes"sv,
                    rule.filter_scopes.values.size() == 1 ? ""sv : "["sv,
                    rule.filter_scopes.values,
                    rule.filter_scopes.values.size() == 1 ? ""sv : "]"sv
                );
            }
            if(!rule.filter_severities.values.empty()) {
                std::vector<std::string_view> severity_names;
                for(const auto& severity : rule.filter_severities.values) {
                    severity_names.push_back(common::log_severity_names[std::to_underlying(severity)]);
                }
                generated_decription += std::format(" {} {} {}{:n}{} and",
                    rule.filter_severities.type == common::filter_type::INCLUDE ? "being of"sv : "not being of"sv,
                    rule.filter_severities.values.size() == 1 ? "severity"sv : "severities"sv,
                    rule.filter_severities.values.size() == 1 ? ""sv : "["sv,
                    severity_names,
                    rule.filter_severities.values.size() == 1 ? ""sv : "]"sv
                );
            }
            if(!rule.filter_attributes.values.empty()) {
                generated_decription += std::format(" {} {} {}{:n}{} and",
                    rule.filter_attributes.type == common::filter_type::INCLUDE ? "having"sv : "not having"sv,
                    rule.filter_attributes.values.size() == 1 ? "attribute"sv : "attributes"sv,
                    rule.filter_attributes.values.size() == 1 ? ""sv : "["sv,
                    rule.filter_attributes.values,
                    rule.filter_attributes.values.size() == 1 ? ""sv : "]"sv
                );
            }
            if(!rule.filter_attributes.values.empty()) {
                generated_decription += std::format(" {} attributes {}",
                    rule.filter_attribute_values.type == common::filter_type::INCLUDE ? "having"sv : "not having"sv,
                    rule.filter_attribute_values.values.dump().value_or("null")
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
                    p{{_class{"text-sm text-base-content/80"}}, std::format("Execution interval: {}", utils::format_duration(rule.execution_interval))},
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
