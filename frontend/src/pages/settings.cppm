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

        auto render_cleanup_rule(const common::cleanup_rule& rule) {
            using namespace Webxx;

            auto checkbox = rule.enabled ?
                input{{_type{"checkbox"}, _class{"toggle toggle-primary"}, _checked{}}} :
                input{{_type{"checkbox"}, _class{"toggle toggle-primary"}}};

            std::string run_info = rule.last_execution ?
                std::format("Last run: {}", *rule.last_execution) :
                "Never run";
            std::string generated_decription = "Delete logs older than " + utils::format_duration(rule.filter_minimum_age);
            if(!rule.filter_resources.values.empty()) {
                generated_decription += std::format(" {} {} {}{:n}{} and",
                    rule.filter_resources.type == common::filter_type::INCLUDE ? "belonging to" : "not belonging to",
                    rule.filter_resources.values.size() == 1 ? "resource" : "resources",
                    rule.filter_resources.values.size() == 1 ? "" : "[",
                    rule.filter_resources.values,
                    rule.filter_resources.values.size() == 1 ? "" : "]"
                );
            }
            if(!rule.filter_scopes.values.empty()) {
                generated_decription += std::format(" {} {} {}{:n}{} and",
                    rule.filter_scopes.type == common::filter_type::INCLUDE ? "having" : "not having",
                    rule.filter_scopes.values.size() == 1 ? "scope" : "scopes",
                    rule.filter_scopes.values.size() == 1 ? "" : "[",
                    rule.filter_scopes.values,
                    rule.filter_scopes.values.size() == 1 ? "" : "]"
                );
            }
            if(!rule.filter_severities.values.empty()) {
                generated_decription += std::format(" {} {} {}{:n}{} and",
                    rule.filter_severities.type == common::filter_type::INCLUDE ? "being of" : "not being of",
                    rule.filter_severities.values.size() == 1 ? "severity" : "severities",
                    rule.filter_severities.values.size() == 1 ? "" : "[",
                    rule.filter_severities.values | std::views::transform([](const auto& severity) {
                        return common::log_severity_names[std::to_underlying(severity)];
                    }),
                    rule.filter_severities.values.size() == 1 ? "" : "]"
                );
            }
            if(!rule.filter_attributes.values.empty()) {
                generated_decription += std::format(" {} {} {}{:n}{} and",
                    rule.filter_attributes.type == common::filter_type::INCLUDE ? "having" : "not having",
                    rule.filter_attributes.values.size() == 1 ? "attribute" : "attributes",
                    rule.filter_attributes.values.size() == 1 ? "" : "[",
                    rule.filter_attributes.values,
                    rule.filter_attributes.values.size() == 1 ? "" : "]"
                );
            }
            if(!rule.filter_attributes.values.empty()) {
                generated_decription += std::format(" {} attributes {}",
                    rule.filter_attribute_values.type == common::filter_type::INCLUDE ? "having" : "not having",
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
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Edit rule"}}, button{{_class{"btn btn-secondary btn-square"},
                        _onClick("document.getElementById('dialog_edit_cleanup_rule').showModal()")},
                    assets::icons::edit
                }},
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Delete rule"}}, button{{_class{"btn btn-secondary btn-square"},
                        _onClick("document.getElementById('dialog_delete_cleanup_rule').showModal()")},
                    assets::icons::delete_
                }},
            };
        }

        Webxx::fragment render_cleanup_rules() {
            common::cleanup_rule example{};
            example.name = "Example rule";
            example.description = "This is an example rule";

            using namespace Webxx;
            return fragment{
                dv{{_class{"flex flex-col gap-4 w-fit mb-4"}},
                    button{{_class{"btn btn-primary"},
                            _onClick("document.getElementById('dialog_add_cleanup_rule').showModal()")},
                        assets::icons::add,
                        "Add cleanup rule"
                    },
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
                }
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
                }
            };
        }
};

}
