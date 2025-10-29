export module frontend.pages:settings;

import std;
import webpp;
import webxx;
import glaze;
import i18n;

import :page;

import common;
import frontend.assets;
import frontend.components;
import frontend.utils;

using namespace mfk::i18n::literals;

namespace frontend::pages {

export class settings : public page {
    private:
        profile_data& profile;
        common::shared_settings& shared_settings;

        r<common::log_entry>& example_entry;
        r<common::logs_attributes_response>& attributes;
        r<common::logs_scopes_response>& scopes;
        r<common::logs_resources_response>& resources;
        common::advanced_stencil_functions stencil_functions;

        common::cleanup_rules_response cleanup_rules;
        common::alert_rules_response alert_rules;
    public:
        settings(profile_data& profile, common::shared_settings& shared_settings, r<common::log_entry>& example_entry, r<common::logs_attributes_response>& attributes, r<common::logs_scopes_response>& scopes, r<common::logs_resources_response>& resources,
            std::vector<std::pair<std::string_view, common::mmdb*>> mmdbs)
            : profile(profile), shared_settings(shared_settings), example_entry{example_entry}, attributes{attributes}, scopes{scopes}, resources{resources}, stencil_functions{std::move(mmdbs)}
        {
            profile.add_callback([this](auto&) { if(is_open) { open(); }});
        }

        webpp::coroutine<void> refresh() {
            auto cleanup_rules_future = webpp::coro::fetch("/api/v1/settings/cleanup_rules", utils::fetch_options)
                .then(std::mem_fn(&webpp::response::co_bytes));
            auto alert_rules_future = webpp::coro::fetch("/api/v1/settings/alert_rules", utils::fetch_options)
                .then(std::mem_fn(&webpp::response::co_bytes));

            if(auto r = glz::read<common::beve_opts, common::cleanup_rules_response>(co_await cleanup_rules_future)) {
                cleanup_rules = *r;
            } else {
                cleanup_rules = common::cleanup_rules_response{};
                webpp::log("Failed to fetch cleanup rules: {}", glz::format_error(r.error()));
            }
            if(auto r = glz::read<common::beve_opts, common::alert_rules_response>(co_await alert_rules_future)) {
                alert_rules = *r;
            } else {
                alert_rules = common::alert_rules_response{};
                webpp::log("Failed to fetch alert rules: {}", glz::format_error(r.error()));
            }

            webpp::get_element_by_id("settings_cleanup_rules")->inner_html(Webxx::render(render_cleanup_rules()));
            webpp::get_element_by_id("settings_alert_rules")->inner_html(Webxx::render(render_alert_rules()));
        }

        void open() override {
            page::open();

            webpp::coro::submit(refresh());
        }

        Webxx::dialog render_cleanup_rule_dialog(event_context& ctx, std::optional<common::cleanup_rule> rule = std::nullopt) {
            return components::dialog_add(ctx, "dialog_add_rule", components::dialog_add_ctx{
                .what = "Cleanup Rule"_s,
                .edit = rule.has_value(),
                .object_id = rule.has_value() ? rule->id : 0,
                .object_name = rule.has_value() ? rule->name : "",
                .object_description = rule.has_value() ? rule->description : "",
                .name_validator = [this](components::dialog_add_ctx& ctx, const std::string& name) -> std::optional<std::string> {
                    if(name.empty()) {
                        return "Rule name must be non-empty."_s;
                    }
                    if(ctx.edit && ctx.object_name == name) {
                        return std::nullopt;
                    }
                    for(const auto& r : cleanup_rules.rules) {
                        if(r.second.name == name) {
                            return "Rule name must be unique."_s;
                        }
                    }
                    return std::nullopt;
                },
                .content = [this, &rule](components::dialog_add_ctx& ctx, auto valid) -> Webxx::fragment {
                    using namespace Webxx;

                    return fragment{
                        dv{{_class{"flex flex-row gap-16 w-full"}},
                            dv{{_class{"flex flex-col"}},
                                label{{_class{"fieldset-label"}}, "Enabled"_},
                                dv{{_class{"flex grow items-center"}},
                                    (ctx.edit && rule->enabled) ?
                                        input{{_id{"dialog_add_rule_enabled"}, _type{"checkbox"}, _class{"toggle toggle-xl toggle-primary"}, _checked{}}} :
                                        input{{_id{"dialog_add_rule_enabled"}, _type{"checkbox"}, _class{"toggle toggle-xl toggle-primary"}}}
                                },
                                dv{{_class{"validator-hint invisible"}}, "Just here for equal spacing."_},
                            },
                            dv{{_class{"grow"}},
                                label{{_class{"fieldset-label"}}, "Execution interval"_},
                                components::duration_picker("dialog_add_rule_execution_interval", "Execution interval"_,
                                    dv{{_class{"validator-hint w-[200%]"}}, "Execution interval must be a positive number."_},
                                    rule.transform([](const auto& v){return v.execution_interval;})
                                )
                            }
                        },
                        fieldset{{_class{"fieldset p-4 rounded-box shadow w-full h-full flex flex-col"}},
                            legend{{_class{"fieldset-legend"}}, "Conditions"_},

                            label{{_class{"fieldset-label"}}, "Minimum age"_},
                            components::duration_picker("dialog_add_rule_minimum_age", "Minimum age"_,
                                dv{{_class{"validator-hint w-[200%]"}}, "Minimum age must be a positive number."_},
                                rule.transform([](const auto& v){return v.filter_minimum_age;})
                            )
                        },
                    };
                },
                .add_callback = [this, rule](components::dialog_add_ctx& ctx, unsigned int id, const std::string& name, const std::string& description,
                    auto success, auto error) -> void {
                    webpp::coro::submit([this](auto rule, auto name, auto description, bool edit, auto success, auto error) -> webpp::coroutine<void> {
                        common::cleanup_rule new_rule = rule.value_or(common::cleanup_rule{});
                        new_rule.name = name;
                        new_rule.description = description;
                        new_rule.enabled = webpp::get_element_by_id("dialog_add_rule_enabled")->get_property<bool>("checked").value_or(false);
                        new_rule.execution_interval = components::get_duration("dialog_add_rule_execution_interval");
                        new_rule.filter_minimum_age = components::get_duration("dialog_add_rule_minimum_age");

                        std::string beve = glz::write<common::beve_opts>(new_rule).value_or("null");

                        webpp::js_object request = webpp::js_object::create();
                        request["headers"] = utils::fetch_headers;
                        request["body"] = webpp::uint8array::create(beve);

                        webpp::response res;
                        if(rule.has_value()) {
                            request["method"] = "PATCH";
                            res = co_await webpp::coro::fetch(std::format("/api/v1/settings/cleanup_rules/{}", rule->id), request);
                        } else {
                            request["method"] = "PUT";
                            res = co_await webpp::coro::fetch("/api/v1/settings/cleanup_rules", request);
                        }

                        if(!res.ok()) {
                            std::string message = co_await res.co_text();
                            error(res.status_text(), message);
                            co_return;
                        }
                        auto new_rule_expected = glz::read<common::beve_opts, common::cleanup_rule>(co_await res.co_bytes());
                        if(!new_rule_expected) {
                            error("Failed to parse rule"_s, glz::format_error(new_rule_expected.error()));
                            co_return;
                        }

                        cleanup_rules.rules[new_rule_expected->id] = *new_rule_expected;
                        webpp::get_element_by_id("settings_cleanup_rules")->inner_html(Webxx::render(render_cleanup_rules()));

                        success();
                        co_return;
                    }(rule, name, description, ctx.edit, std::move(success), std::move(error)));
                }
            });
        }

        Webxx::dialog render_alert_rule_dialog(event_context& ctx, std::optional<common::alert_rule> rule = std::nullopt) {
            return components::dialog_add(ctx, "dialog_add_rule", components::dialog_add_ctx{
                .what = "Alert Rule"_s,
                .edit = rule.has_value(),
                .object_id = rule.has_value() ? rule->id : 0,
                .object_name = rule.has_value() ? rule->name : "",
                .object_description = rule.has_value() ? rule->description : "",
                .name_validator = [this](components::dialog_add_ctx& ctx, const std::string& name) -> std::optional<std::string> {
                    if(name.empty()) {
                        return "Rule name must be non-empty."_s;
                    }
                    if(ctx.edit && ctx.object_name == name) {
                        return std::nullopt;
                    }
                    for(const auto& r : alert_rules.rules) {
                        if(r.second.name == name) {
                            return "Rule name must be unique."_s;
                        }
                    }
                    return std::nullopt;
                },
                .content = [this, &rule, &ctx](components::dialog_add_ctx& dctx, auto valid) -> Webxx::fragment {
                    using namespace Webxx;

                    static auto render_provider_options = [](const common::notification_provider_info& info, std::optional<common::alert_rule> rule) -> Webxx::fragment {
                        return fragment{each(info.options, [&](const common::notification_provider_option& option) {
                            fragment input_field = [&]() -> fragment {
                                std::optional<glz::json_t> value = option.default_value;
                                if(rule && rule->notification_options.contains(option.id)) {
                                    value = rule->notification_options[option.id];
                                }
                                switch(option.type) {
                                    case common::notification_provider_option_type::STRING:
                                        return fragment{input{{_id{std::format("dialog_add_rule_notification_option_{}", option.id)},
                                            _type{"text"}, _class{"input w-full"}, _placeholder{option.description}, _value{value.value_or("").get_string()}}}};
                                    case common::notification_provider_option_type::OBJECT:
                                        return fragment{textarea{{_id{std::format("dialog_add_rule_notification_option_{}", option.id)},
                                            _class{"input w-full"}, _placeholder{option.description}},
                                            glz::write<glz::opts{.prettify=true}>(value.value_or(glz::json_t::object_t{})).value_or("{}")
                                        }};
                                    case common::notification_provider_option_type::ARRAY:
                                        return fragment{textarea{{_id{std::format("dialog_add_rule_notification_option_{}", option.id)},
                                            _class{"input w-full"}, _placeholder{option.description}},
                                            glz::write<glz::opts{.prettify=true}>(value.value_or(glz::json_t::array_t{})).value_or("[]")
                                        }};
                                    default:
                                        return fragment{};
                                }
                            }();

                            return dv{{_class{"flex flex-col gap-2"}},
                                label{{_class{"fieldset-label"}}, option.name},
                                std::move(input_field),
                            };
                        })};
                    };

                    return fragment{
                        dv{{_class{"flex flex-row gap-16 w-full"}},
                            dv{{_class{"flex flex-col"}},
                                label{{_class{"fieldset-label"}}, "Enabled"_},
                                dv{{_class{"flex grow items-center"}},
                                    (dctx.edit && rule->enabled) ?
                                        input{{_id{"dialog_add_rule_enabled"}, _type{"checkbox"}, _class{"toggle toggle-xl toggle-primary"}, _checked{}}} :
                                        input{{_id{"dialog_add_rule_enabled"}, _type{"checkbox"}, _class{"toggle toggle-xl toggle-primary"}}}
                                },
                                dv{{_class{"validator-hint invisible"}}, "&nbsp;"},
                            },
                            dv{{_class{"grow"}},
                                label{{_class{"fieldset-label"}}, "Notification Provider"_},
                                ctx.on_change(select{{_id{"dialog_add_rule_notification_provider"}, _class{"select w-full"}},
                                    option{{_class{"italic"}, _value{""}}, "Select a provider"_},
                                    each(shared_settings.notification_providers, [&](const auto& provider) {
                                        if(dctx.edit && provider.first == rule->notification_provider) {
                                            return option{{_value{provider.first}, _selected{}}, provider.second.name};
                                        }
                                        return option{{_value{provider.first}}, provider.second.name};
                                    }),
                                }, [this, valid, rule](webpp::event e){
                                    auto selected = e.target().get_property<std::string>("value").value_or("");
                                    if(selected.empty() || !shared_settings.notification_providers.contains(selected)) {
                                        valid(false);
                                        webpp::get_element_by_id("dialog_add_rule_notification_options_error")->remove_class("hidden");
                                        webpp::get_element_by_id("dialog_add_rule_notification_options")->inner_html("");

                                        return;
                                    }
                                    valid(true);
                                    webpp::get_element_by_id("dialog_add_rule_notification_options_error")->add_class("hidden");
                                    const auto& info = shared_settings.notification_providers[selected];

                                    auto options = render_provider_options(info,
                                        selected == rule.transform([](const auto& r) { return r.notification_provider; }) ? rule : std::nullopt);
                                    webpp::get_element_by_id("dialog_add_rule_notification_options")->inner_html(Webxx::render(options));
                                })
                            }
                        },
                        fieldset{{_class{"fieldset p-4 rounded-box shadow w-full h-full flex flex-col"}},
                            legend{{_class{"fieldset-legend"}}, "Notification Settings"_},
                            dv{{_id{"dialog_add_rule_notification_options_error"}, _class{dctx.edit && !rule->notification_provider.empty() ? "hidden" : ""}},
                                p{{_class{"text-error"}}, "Please select a notification provider first."_},
                            },
                            dv{{_id{"dialog_add_rule_notification_options"}, _class{"flex flex-col gap-4"}},
                                maybe(dctx.edit && !rule->notification_provider.empty(), [this, &rule](){
                                    return render_provider_options(
                                        shared_settings.notification_providers[rule->notification_provider], rule);
                                })
                            },
                        },
                        fieldset{{_class{"fieldset p-4 rounded-box shadow w-full h-full flex flex-col"}},
                            legend{{_class{"fieldset-legend"}}, "Conditions"_},
                        },
                    };
                },
                .add_callback = [this, rule](components::dialog_add_ctx& ctx, unsigned int id, const std::string& name, const std::string& description,
                    auto success, auto error) -> void {
                    webpp::coro::submit([this](auto rule, auto name, auto description, bool edit, auto success, auto error) -> webpp::coroutine<void> {
                        common::alert_rule new_rule = rule.value_or(common::alert_rule{});
                        new_rule.name = name;
                        new_rule.description = description;
                        new_rule.enabled = webpp::get_element_by_id("dialog_add_rule_enabled")->get_property<bool>("checked").value_or(false);
                        new_rule.notification_provider = webpp::get_element_by_id("dialog_add_rule_notification_provider")->get_property<std::string>("value").value_or("");

                        const auto& info = shared_settings.notification_providers[new_rule.notification_provider];
                        new_rule.notification_options = glz::json_t::object_t{};
                        for(const auto& option : info.options) {
                            auto value = webpp::get_element_by_id(std::format("dialog_add_rule_notification_option_{}", option.id))
                                ->get_property<std::string>("value").value_or("");
                            if(value.empty()) {
                                continue; // Skip empty values
                            }
                            switch(option.type) {
                                case common::notification_provider_option_type::STRING:
                                    new_rule.notification_options[option.id] = value;
                                    break;
                                case common::notification_provider_option_type::OBJECT: {
                                    auto v = glz::read_json<glz::json_t>(value);
                                    if(!v) {
                                        error("Invalid JSON for option {}"_(option.name), glz::format_error(v.error()));
                                        co_return;
                                    }
                                    if(!v->is_object()) {
                                        error("Invalid JSON for option {}"_(option.name), "Expected an object.");
                                        co_return;
                                    }
                                    new_rule.notification_options[option.id] = *v;
                                    break;
                                }
                                case common::notification_provider_option_type::ARRAY: {
                                    auto v = glz::read_json<glz::json_t>(value);
                                    if(!v) {
                                        error("Invalid JSON for option {}"_(option.name), glz::format_error(v.error()));
                                        co_return;
                                    }
                                    if(!v->is_array()) {
                                        error("Invalid JSON for option {}"_(option.name), "Expected an array."_s);
                                        co_return;
                                    }
                                    new_rule.notification_options[option.id] = *v;
                                    break;
                                }
                                default:
                                    error("Unknown option type"_s, "The notification provider option type is not supported yet."_s);
                                    co_return;
                            }
                        }

                        std::string beve = glz::write<common::beve_opts>(new_rule).value_or("null");

                        webpp::js_object request = webpp::js_object::create();
                        request["headers"] = utils::fetch_headers;
                        request["body"] = webpp::uint8array::create(beve);

                        webpp::response res;
                        if(rule.has_value()) {
                            request["method"] = "PATCH";
                            res = co_await webpp::coro::fetch(std::format("/api/v1/settings/alert_rules/{}", rule->id), request);
                        } else {
                            request["method"] = "PUT";
                            res = co_await webpp::coro::fetch("/api/v1/settings/alert_rules", request);
                        }

                        if(!res.ok()) {
                            std::string message = co_await res.co_text();
                            error(res.status_text(), message);
                            co_return;
                        }
                        auto new_rule_expected = glz::read<common::beve_opts, common::alert_rule>(co_await res.co_bytes());
                        if(!new_rule_expected) {
                            error("Failed to parse rule"_s, glz::format_error(new_rule_expected.error()));
                            co_return;
                        }

                        alert_rules.rules[new_rule_expected->id] = *new_rule_expected;
                        webpp::get_element_by_id("settings_alert_rules")->inner_html(Webxx::render(render_alert_rules()));

                        success();
                        co_return;
                    }(rule, name, description, ctx.edit, std::move(success), std::move(error)));
                },
                .user_valid = false,
            });
        }

        Webxx::dialog render_rule_delete_dialog(event_context& ctx, const std::string& what, const std::string& path, auto rule, auto& rules) {
            using namespace Webxx;

            return components::dialog_delete(ctx, "dialog_delete_rule", what, rule.id, rule.name,
                [this, path, &rules](unsigned int id, auto success, auto error) -> void {
                    webpp::coro::submit([this](auto id, auto success, auto error, auto path, auto& rules) -> webpp::coroutine<void> {
                        webpp::js_object request = webpp::js_object::create();
                        request["headers"] = utils::fetch_headers;
                        request["method"] = "DELETE";

                        webpp::response res = co_await webpp::coro::fetch(std::format("{}/{}", path, id), request);
                        if(!res.ok()) {
                            std::string message = co_await res.co_text();
                            error(res.status_text(), message);
                            co_return;
                        }

                        rules.rules.erase(id);
                        webpp::get_element_by_id("settings_cleanup_rules")->inner_html(Webxx::render(render_cleanup_rules()));
                        webpp::get_element_by_id("settings_alert_rules")->inner_html(Webxx::render(render_alert_rules()));

                        success();
                        co_return;
                    }(id, std::move(success), std::move(error), std::move(path), rules));
                }
            );
        }

        void edit_rule(const common::cleanup_rule& rule) {
            static event_context ctx;
            ctx.clear();

            webpp::get_element_by_id("dialog_placeholder")->inner_html(Webxx::render(render_cleanup_rule_dialog(ctx, rule)));
            webpp::eval("document.getElementById('dialog_add_rule').showModal();");
        }
        void delete_rule(const common::cleanup_rule& rule) {
            static event_context ctx; ctx.clear();
            webpp::get_element_by_id("dialog_placeholder")->inner_html(Webxx::render(
                render_rule_delete_dialog(ctx, "Cleanup Rule"_s, "/api/v1/settings/cleanup_rules", rule, cleanup_rules)));
            webpp::eval("document.getElementById('dialog_delete_rule').showModal();");
        }
        void toggle_rule(common::cleanup_rule rule) {
            rule.enabled = !rule.enabled; // Toggle the enabled state
            webpp::coro::submit([this, rule]() -> webpp::coroutine<void> {
                webpp::js_object request = webpp::js_object::create();
                request["headers"] = utils::fetch_headers;
                request["method"] = "PATCH";
                request["body"] = webpp::uint8array::create(glz::write<common::beve_opts>(rule).value_or("null"));

                webpp::response res = co_await webpp::coro::fetch(std::format("/api/v1/settings/cleanup_rules/{}", rule.id), request);
                if(!res.ok()) {
                    std::string message = co_await res.co_text();
                    components::show_alert("settings_cleanup_rules_error", res.status_text(), message);
                    co_return;
                }
                auto new_rule_expected = glz::read<common::beve_opts, common::cleanup_rule>(co_await res.co_bytes());
                if(!new_rule_expected) {
                    components::show_alert("settings_cleanup_rules_error", "Failed to parse rule", glz::format_error(new_rule_expected.error()));
                    co_return;
                }

                cleanup_rules.rules[new_rule_expected->id] = *new_rule_expected;
                webpp::get_element_by_id("settings_cleanup_rules")->inner_html(Webxx::render(render_cleanup_rules()));

                co_return;
            }());
        }

        void edit_rule(const common::alert_rule& rule) {
            static event_context ctx;
            ctx.clear();

            webpp::get_element_by_id("dialog_placeholder")->inner_html(Webxx::render(render_alert_rule_dialog(ctx, rule)));
            webpp::eval("document.getElementById('dialog_add_rule').showModal();");
        }
        void delete_rule(const common::alert_rule& rule) {
            static event_context ctx; ctx.clear();
            webpp::get_element_by_id("dialog_placeholder")->inner_html(Webxx::render(
                render_rule_delete_dialog(ctx, "Alert Rule", "/api/v1/settings/alert_rules", rule, alert_rules)));
            webpp::eval("document.getElementById('dialog_delete_rule').showModal();");
        }
        void toggle_rule(common::alert_rule rule) {
            rule.enabled = !rule.enabled; // Toggle the enabled state
            webpp::coro::submit([this, rule]() -> webpp::coroutine<void> {
                webpp::js_object request = webpp::js_object::create();
                request["headers"] = utils::fetch_headers;
                request["method"] = "PATCH";
                request["body"] = webpp::uint8array::create(glz::write<common::beve_opts>(rule).value_or("null"));

                webpp::response res = co_await webpp::coro::fetch(std::format("/api/v1/settings/alert_rules/{}", rule.id), request);
                if(!res.ok()) {
                    std::string message = co_await res.co_text();
                    components::show_alert("settings_alert_rules_error", res.status_text(), message);
                    co_return;
                }
                auto new_rule_expected = glz::read<common::beve_opts, common::alert_rule>(co_await res.co_bytes());
                if(!new_rule_expected) {
                    components::show_alert("settings_alert_rules_error", "Failed to parse rule"_s, glz::format_error(new_rule_expected.error()));
                    co_return;
                }

                alert_rules.rules[new_rule_expected->id] = *new_rule_expected;
                webpp::get_element_by_id("settings_alert_rules")->inner_html(Webxx::render(render_alert_rules()));

                co_return;
            }());
        }

        struct rule_callbacks {
            public:
                rule_callbacks(settings* s, auto& rules) {
                    edit = std::make_unique<webpp::callback_data>(
                        [s, &rules](js_handle h, std::string_view) {trigger_edit_rule(s, rules, h);}, false);
                    delete_ = std::make_unique<webpp::callback_data>(
                        [s, &rules](js_handle h, std::string_view) {trigger_delete_rule(s, rules, h);}, false);
                    toggle = std::make_unique<webpp::callback_data>(
                        [s, &rules](js_handle h, std::string_view) {trigger_toggle_rule(s, rules, h);}, false);
                }

                std::unique_ptr<webpp::callback_data> edit;
                std::unique_ptr<webpp::callback_data> delete_;
                std::unique_ptr<webpp::callback_data> toggle;
            private:
                using js_handle = decltype(std::declval<webpp::js_object>().handle());
                static void trigger_edit_rule(settings* s, auto& rules, js_handle h) {
                    webpp::event event{h};
                    webpp::element target = *event.current_target().as<webpp::element>();
                    unsigned int id = std::stoi(*target["dataset"]["ruleId"].as<std::string>());
                    const auto& rule = rules.rules[id];
                    s->edit_rule(rule);
                }
                static void trigger_delete_rule(settings* s, auto& rules, js_handle h) {
                    webpp::event event{h};
                    webpp::element target = *event.current_target().as<webpp::element>();
                    unsigned int id = std::stoi(*target["dataset"]["ruleId"].as<std::string>());
                    const auto& rule = rules.rules[id];
                    s->delete_rule(rule);
                }
                static void trigger_toggle_rule(settings* s, auto& rules, js_handle h) {
                    webpp::event event{h};
                    webpp::element target = *event.current_target().as<webpp::element>();
                    unsigned int id = std::stoi(*target["dataset"]["ruleId"].as<std::string>());
                    const auto& rule = rules.rules[id];
                    s->toggle_rule(rule);
                }
        };
        friend struct rule_callbacks;

        rule_callbacks cb_cleanup_rules{this, cleanup_rules};
        rule_callbacks cb_alert_rules{this, alert_rules};

        std::string describe_filters(const common::standard_filters& filters) {
            // TODO: localize this
            using std::string_view_literals::operator""sv;

            std::string generated_decription{};
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

            add_filter("resource"sv, "resources"sv, "having"sv, "not having"sv, filters.resources);
            add_filter("scope"sv, "scopes"sv, "having"sv, "not having"sv, filters.scopes);
            add_filter("severity"sv, "severities"sv, "being of"sv, "not being of"sv, filters.severities);
            add_filter("attribute"sv, "attributes"sv, "having"sv, "not having"sv, filters.attributes);
            if(!filters.attribute_values.values.empty()) {
                generated_decription += std::format(" {} attributes {}",
                    filters.attribute_values.type == common::filter_type::INCLUDE ? "having"sv : "not having"sv,
                    filters.attribute_values.values.dump().value_or("null")
                );
            }
            if(generated_decription.ends_with(" and")) {
                generated_decription = generated_decription.substr(0, generated_decription.size() - 4);
            }
            return generated_decription;
        }

        auto render_cleanup_rule(const common::cleanup_rule& rule) {
            using namespace Webxx;

            constexpr static char dataRuleIdAttr[] = "data-rule-id";
            using _dataRuleId = Webxx::attr<dataRuleIdAttr>;

            auto checkbox = rule.enabled ?
                input{{_type{"checkbox"}, _class{"toggle toggle-primary"}, _checked{}}} :
                input{{_type{"checkbox"}, _class{"toggle toggle-primary"}}};
            checkbox.data.attributes.emplace_back(_dataRuleId{std::to_string(rule.id)});
            checkbox = events::on_change(std::move(checkbox), cb_cleanup_rules.toggle.get());

            std::string run_info = rule.last_execution ?
                "Last run: {}"_(*rule.last_execution) :
                "Never run"_s;
            std::string generated_decription = "Delete logs older than {}"_(common::format_duration(rule.filter_minimum_age));
            generated_decription += describe_filters(rule.filters);
            generated_decription += ".";

            return li{{_class{"list-row items-center"}},
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Enable/Disable rule"_}}, std::move(checkbox)},
                dv{{_class{"list-col-grow flex flex-row items-center gap-4"}},
                    h3{{_class{"text-lg font-bold"}}, rule.name},
                    p{{_class{"text-sm text-base-content/80"}}, "Execution interval: {}"_(common::format_duration(rule.execution_interval))},
                },
                dv{{_class{"text-sm text-base-content/80 list-col-wrap"}},
                    p{rule.description.empty() ? fragment{i{"No description"_}} : fragment{b{rule.description}}},
                    p{generated_decription}
                },
                p{{_class{"text-sm text-base-content/80"}}, run_info},
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Edit rule"_}},
                    events::on_click(button{{_class{"btn btn-secondary btn-square"}, _dataRuleId{std::to_string(rule.id)}}, assets::icons::edit}, cb_cleanup_rules.edit.get()),
                },
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Delete rule"_}},
                    events::on_click(button{{_class{"btn btn-secondary btn-square"}, _dataRuleId{std::to_string(rule.id)}}, assets::icons::delete_}, cb_cleanup_rules.delete_.get()),
                },
            };
        }

        auto render_alert_rule(const common::alert_rule& rule) {
            using namespace Webxx;

            constexpr static char dataRuleIdAttr[] = "data-rule-id";
            using _dataRuleId = Webxx::attr<dataRuleIdAttr>;

            auto checkbox = rule.enabled ?
                input{{_type{"checkbox"}, _class{"toggle toggle-primary"}, _checked{}}} :
                input{{_type{"checkbox"}, _class{"toggle toggle-primary"}}};
            checkbox.data.attributes.emplace_back(_dataRuleId{std::to_string(rule.id)});
            checkbox = events::on_change(std::move(checkbox), cb_alert_rules.toggle.get());

            std::string run_info = rule.last_alert ?
                "Last alert: {}"_(*rule.last_alert) :
                "Never triggered"_s;
            std::string generated_decription = "Alert for logs";
            generated_decription += describe_filters(rule.filters);
            generated_decription += ".";

            return li{{_class{"list-row items-center"}},
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Enable/Disable rule"_}}, std::move(checkbox)},
                dv{{_class{"list-col-grow flex flex-row items-center gap-4"}},
                    h3{{_class{"text-lg font-bold"}}, rule.name},
                    p{{_class{"text-sm text-base-content/80"}}, "Provider: {}"_(rule.notification_provider)},
                },
                dv{{_class{"text-sm text-base-content/80 list-col-wrap"}},
                    p{rule.description.empty() ? fragment{i{"No description"_}} : fragment{b{rule.description}}},
                    p{generated_decription}
                },
                p{{_class{"text-sm text-base-content/80"}}, run_info},
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Edit rule"_}},
                    events::on_click(button{{_class{"btn btn-secondary btn-square"}, _dataRuleId{std::to_string(rule.id)}}, assets::icons::edit}, cb_alert_rules.edit.get()),
                },
                dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Delete rule"_}},
                    events::on_click(button{{_class{"btn btn-secondary btn-square"}, _dataRuleId{std::to_string(rule.id)}}, assets::icons::delete_}, cb_alert_rules.delete_.get()),
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
                    ctx.on_click(button{{_class{"btn btn-primary"}}, assets::icons::add, "Add cleanup rule"_},
                        [this](webpp::event e) {
                            static event_context sctx;
                            sctx.clear();

                            webpp::get_element_by_id("dialog_placeholder")->inner_html(Webxx::render(render_cleanup_rule_dialog(sctx)));
                            webpp::eval("document.getElementById('dialog_add_rule').showModal();");
                        })
                },
                ul{{_class{"list bg-base-200 rounded-box shadow-md"}},
                    each(cleanup_rules.rules, [&](const auto& rule) {
                        return render_cleanup_rule(rule.second);
                    }),
                    maybe(cleanup_rules.rules.empty(), [&]() {
                        return li{{_class{"list-row items-center"}},
                            h3{{_class{"text-lg font-bold text-base-content/80"}}, "No cleanup rules found"_},
                            p{{_class{"text-sm text-base-content/80 list-col-grow"}}, "You can add a new rule by clicking the button above."_}
                        };
                    })
                },
            };
        }

        Webxx::fragment render_alert_rules() {
            static event_context ctx;
            ctx.clear();

            using namespace Webxx;
            return fragment{
                components::alert("settings_alert_rules_error", "mb-4"),
                dv{{_class{"flex flex-col gap-4 w-fit mb-4"}},
                    ctx.on_click(button{{_class{"btn btn-primary"}}, assets::icons::add, "Add alert rule"_},
                        [this](webpp::event e) {
                            static event_context sctx;
                            sctx.clear();

                            webpp::get_element_by_id("dialog_placeholder")->inner_html(Webxx::render(render_alert_rule_dialog(sctx)));
                            webpp::eval("document.getElementById('dialog_add_rule').showModal();");
                        })
                },
                ul{{_class{"list bg-base-200 rounded-box shadow-md"}},
                    each(alert_rules.rules, [&](const auto& rule) {
                        return render_alert_rule(rule.second);
                    }),
                    maybe(alert_rules.rules.empty(), [&]() {
                        return li{{_class{"list-row items-center"}},
                            h3{{_class{"text-lg font-bold text-base-content/80"}}, "No alert rules found"_},
                            p{{_class{"text-sm text-base-content/80 list-col-grow"}}, "You can add a new rule by clicking the button above."_}
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
                    input{{_type{"radio"}, _name{"settings_tab"}, _class{"tab"}, _ariaLabel{"Cleanup rules"_}, _checked{}}},
                    dv{{_id{"settings_cleanup_rules"}, _class{"tab-content border-base-300 bg-base-100 p-10"}}, render_cleanup_rules()},

                    input{{_type{"radio"}, _name{"settings_tab"}, _class{"tab"}, _ariaLabel{"Alert rules"_}}},
                    dv{{_id{"settings_alert_rules"}, _class{"tab-content border-base-300 bg-base-100 p-10"}}, render_alert_rules()},
                },
                dv{{_id{"dialog_placeholder"}}},
            };
        }
};

}
