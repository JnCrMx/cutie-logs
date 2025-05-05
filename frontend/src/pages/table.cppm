export module frontend.pages:table;

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

export class table : public page {
    private:
        std::string build_attributes_selector(const std::unordered_map<std::string, bool>& selected_attributes) {
            std::string attributes_selector{};
            for(const auto& [attr, selected] : selected_attributes) {
                if(selected) {
                    attributes_selector += std::format("{},", attr);
                }
            }
            if(!attributes_selector.empty()) {
                attributes_selector.pop_back();
            }
            return attributes_selector;
        }
        std::string build_scopes_selector(const std::unordered_map<std::string, bool>& selected_scopes) {
            std::string scopes_selector{};
            for(const auto& [scope, selected] : selected_scopes) {
                if(selected) {
                    scopes_selector += std::format("{},", scope.empty() ? "<empty>" : scope);
                }
            }
            return scopes_selector+"<dummy>";
        }
        std::string build_resources_selector(const std::unordered_map<std::string, bool>& selected_resources) {
            std::string resources_selector{};
            for(const auto& [res, selected] : selected_resources) {
                if(selected) {
                    resources_selector += std::format("{},", res);
                }
            }
            return resources_selector+"0";
        }

        unsigned int dragged_column = 0;
        unsigned int dragged_position = 0;
        common::logs_response logs;
        std::vector<std::string> table_column_order;
        std::map<std::string, std::string> table_custom_columns;

        Webxx::fragment make_table() {
            using namespace Webxx;
            table_column_order = make_table_order(table_column_order);

            static event_context ctx;
            ctx.clear();

            auto dragstart = [this](webpp::event e) {
                auto el = *e.current_target().as<webpp::element>();
                int column_pos = std::stoi(*el["dataset"]["columnPos"].as<std::string>());
                dragged_column = column_pos;
                dragged_position = column_pos;
            };
            auto dragover = [this](webpp::event e) {
                auto el = *e.current_target().as<webpp::element>();
                int x = *e["offsetX"].as<int>();
                int w = el.offset_width();
                int column_pos = std::stoi(*el["dataset"]["columnPos"].as<std::string>());

                if(x > w / 2) {
                    el.add_class("border-r-4");
                    el.remove_class("border-l-4");
                    dragged_position = column_pos + 1;
                } else {
                    el.add_class("border-l-4");
                    el.remove_class("border-r-4");
                    dragged_position = column_pos;
                }
            };
            auto dragleave = [](webpp::event e) {
                auto el = *e.current_target().as<webpp::element>();
                el.remove_class("border-l-4");
                el.remove_class("border-r-4");
            };
            auto dragend = [this](webpp::event e) {
                if(dragged_position > dragged_column) {
                    dragged_position--;
                }
                if(dragged_position == dragged_column) {
                    return;
                }

                if(dragged_column < dragged_position) {
                    std::rotate(table_column_order.begin() + dragged_column, table_column_order.begin() + dragged_column + 1, table_column_order.begin() + dragged_position + 1);
                } else if(dragged_column > dragged_position) {
                    std::rotate(table_column_order.begin() + dragged_position, table_column_order.begin() + dragged_column, table_column_order.begin() + dragged_column + 1);
                }
                webpp::set_timeout(std::chrono::milliseconds(0), std::bind(&table::render_table, this));
            };

            unsigned int column_pos = 0;
            auto make_th = [&](std::string text) {
                constexpr static char dataColumnPosAttr[] = "data-column-pos";
                using _dataColumnPos = Webxx::attr<dataColumnPosAttr>;

                if(text.starts_with(':') || text.starts_with("^")) {
                    text = text.substr(1);
                }

                std::string_view heading_classes = "cursor-grab hover:bg-base-300 box-border";
                return ctx.on_events<drag_start_event, drag_end_event, drag_over_event, drag_leave_event>(
                        th{{_draggable{"true"}, _class{heading_classes}, _dataColumnPos{std::to_string(column_pos++)}}, text},
                    dragstart, dragend, dragover, dragleave);
            };

            auto view = Webxx::table{{_class{"table table-pin-rows bg-base-200"}},
                thead{tr{
                    each(table_column_order, [&](const std::string& attr) { return make_th(attr); })
                }},
                each(logs.logs, [&, this](const common::log_entry& entry) {
                    using sys_seconds_double = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>;
                    auto timestamp_double = sys_seconds_double{std::chrono::duration<double>{entry.timestamp}};
                    auto timestamp = std::chrono::time_point_cast<std::chrono::sys_seconds::duration>(timestamp_double);

                    auto attributes = entry.attributes.is_object() ? entry.attributes.get_object() : glz::json_t::object_t{};
                    auto e_timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", timestamp);
                    auto e_resource = a{{_class{"link"}, _onClick{std::format("document.getElementById('modal_resource_{}').showModal()", entry.resource)}},
                        resource_name(entry.resource, std::get<0>(resources->resources[entry.resource]))};
                    auto e_scope = entry.scope;
                    auto e_severity = common::log_severity_names[std::to_underlying(entry.severity)];
                    auto e_body = entry.body.is_string() ? entry.body.get_string() : entry.body.dump().value_or("error");

                    return tr{
                        each(table_column_order, [&](const std::string& attr) {
                            if(attr == ":Timestamp") {
                                return td{e_timestamp};
                            } else if(attr == ":Resource") {
                                return td{e_resource};
                            } else if(attr == ":Scope") {
                                return td{e_scope};
                            } else if(attr == ":Severity") {
                                return td{e_severity};
                            } else if(attr == ":Body") {
                                return td{e_body};
                            }
                            if(attr.starts_with("^")) {
                                auto name = attr.substr(1);
                                if(table_custom_columns.contains(name)) {
                                    const auto& stencil = table_custom_columns.at(name);
                                    auto r = common::stencil(stencil, entry, stencil_functions);
                                    if(r) {
                                        return td{*r};
                                    } else {
                                        return td{{_class{"text-error"}}, std::format("error: {}", r.error())};
                                    }
                                }
                            }

                            if(!attributes.contains(attr)) {
                                return td{{_class{"text-error"}}, "missing"};
                            }
                            return td{attributes.at(attr).is_string() ? attributes.at(attr).get_string() : attributes.at(attr).dump().value_or("error")};
                        })
                    };
                })
            };
            return fragment{view};
        }

        std::vector<std::string> make_table_order(std::vector<std::string> initial = {}) const {
            std::vector<std::string>& order = initial;

            std::set<std::string> attributes_set;
            for(const auto& [attr, selected] : selected_attributes) {
                if(selected) {
                    attributes_set.insert(attr);
                }
            }
            order.erase(std::remove_if(order.begin(), order.end(),
                [&](const std::string& e){
                    if(e.starts_with(":")) return false;
                    if(e.starts_with("^")) return !table_custom_columns.contains(e.substr(1));
                    return !attributes_set.contains(e);
                }), order.end());

            if(order.empty()) {
                order = {":Timestamp", ":Resource", ":Scope", ":Severity"};
            }
            for(const auto& a : attributes_set) {
                if(std::find(order.begin(), order.end(), a) == order.end()) {
                    order.push_back(a);
                }
            }
            for(auto& [name, stencil] : table_custom_columns) {
                std::string n = "^"+name;
                if(std::find(order.begin(), order.end(), n) == order.end()) {
                    order.push_back(n);
                }
            }
            for(const auto& entry : logs.logs) {
                if(!entry.body.is_null() && (!entry.body.is_string() || !entry.body.get_string().empty())) {
                    if(std::find(order.begin(), order.end(), ":Body") == order.end()) {
                        order.push_back(":Body");
                    }
                    break;
                }
            }
            return order;
        }

        void render_table() {
            webpp::get_element_by_id("table")->inner_html(Webxx::render(make_table()));

            if(table_column_order == make_table_order()) {
                webpp::get_element_by_id("reset_table_button")->add_class("btn-disabled");

                profile.remove_data("table_columns"); // remove the saved order (and thereby set it to default)
            } else {
                webpp::get_element_by_id("reset_table_button")->remove_class("btn-disabled");

                auto order = glz::write_json(table_column_order).value_or("[]");
                profile.set_data("table_columns", order);
            }
        }

        webpp::coroutine<void> run_query() {
            std::string attributes_selector = build_attributes_selector(selected_attributes);
            std::string scopes_selector = build_scopes_selector(selected_scopes);
            std::string resources_selector = build_resources_selector(selected_resources);
            constexpr unsigned int limit = 100;

            auto url = std::format("/api/v1/logs?limit={}&attributes={}&scopes={}&resources={}",
                limit, attributes_selector, scopes_selector, resources_selector);
            logs =
                glz::read_beve<common::logs_response>(co_await webpp::coro::fetch(url, utils::fetch_options).then(std::mem_fn(&webpp::response::co_bytes)))
                .value_or(common::logs_response{});

            webpp::get_element_by_id("run_button_icon")->remove_class("hidden");
            webpp::get_element_by_id("run_button_loading")->add_class("hidden");

            render_table();

            co_return;
        };

        void update_attributes() {
            selected_attributes.clear();
            std::transform(attributes->attributes.begin(), attributes->attributes.end(), std::inserter(selected_attributes, selected_attributes.end()),
                [](const auto& attr) { return std::pair{attr.first, false}; }); // all attributes are deselected by default
            webpp::get_element_by_id("attributes")->inner_html(Webxx::render(
                components::selection<"attributes">("Select Attributes",
                    attributes->attributes, selected_attributes, &profile, attributes->total_logs, true)));
        }
        void update_scopes() {
            selected_scopes.clear();
            std::transform(scopes->scopes.begin(), scopes->scopes.end(), std::inserter(selected_scopes, selected_scopes.end()),
                [](const auto& scope) { return std::pair{scope.first, true}; }); // all scopes are selected by default
            webpp::get_element_by_id("scopes")->inner_html(Webxx::render(
                components::selection<"scopes">("Filter Scopes", scopes->scopes, selected_scopes, &profile, 1, false)));
        }

        std::string resource_name(unsigned int id, const common::log_resource& resource) {
            // TODO: make this configurable
            if(resource.attributes.contains("service.name")) {
                return resource.attributes.at("service.name").get_string();
            }
            if(resource.attributes.contains("k8s.namespace.name")) {
                if(resource.attributes.contains("k8s.pod.name")) {
                    return std::format("{}/{}", resource.attributes.at("k8s.namespace.name").get_string(), resource.attributes.at("k8s.pod.name").get_string());
                }
            }
            return std::format("Resource #{}", id);
        }
        void update_resources() {
            transformed_resources.clear();
            for(const auto& [id, e] : resources->resources) {
                transformed_resources[std::to_string(id)] = {resource_name(id, std::get<0>(e)), std::get<1>(e)};
            }
            selected_resources.clear();
            std::transform(transformed_resources.begin(), transformed_resources.end(), std::inserter(selected_resources, selected_resources.end()),
                [](const auto& res) { return std::pair{res.first, false}; }); // all resources are deselected by default
            webpp::get_element_by_id("resources")->inner_html(Webxx::render(
                components::selection_detail<"resources">("Filter Resources", transformed_resources, selected_resources, &profile, 1, false, "resource")));
        }

        profile_data& profile;
        r<common::log_entry>& example_entry;
        r<common::logs_attributes_response>& attributes;
        r<common::logs_scopes_response>& scopes;
        r<common::logs_resources_response>& resources;
        common::advanced_stencil_functions stencil_functions;

        std::string stencil_format;
        std::unordered_map<std::string, bool> selected_attributes, selected_resources, selected_scopes;
        std::unordered_map<std::string, std::tuple<std::string, unsigned int>> transformed_resources;
    public:
        table(profile_data& profile, r<common::log_entry>& example_entry, r<common::logs_attributes_response>& attributes, r<common::logs_scopes_response>& scopes, r<common::logs_resources_response>& resources,
            std::vector<std::pair<std::string_view, common::mmdb*>> mmdbs)
            : profile(profile), example_entry{example_entry}, attributes{attributes}, scopes{scopes}, resources{resources}, stencil_functions{std::move(mmdbs)}
        {
            profile.add_callback([this](auto&) { if(is_open) { open(); }});
            attributes.add_callback([this](auto&) { if(is_open) update_attributes(); });
            scopes.add_callback([this](auto&) { if(is_open) update_scopes(); });
            resources.add_callback([this](auto&) { if(is_open) update_resources(); });
        }

        void open() override {
            page::open();

            update_attributes();
            update_scopes();
            update_resources();

            auto saved_order = profile.get_data("table_columns").value_or("[]");
            table_column_order = glz::read_json<std::vector<std::string>>(saved_order).value_or(std::vector<std::string>{});

            auto saved_custom_columns = profile.get_data("table_custom_columns").value_or("{}");
            table_custom_columns = glz::read_json<std::map<std::string, std::string>>(saved_custom_columns).value_or(std::map<std::string, std::string>{});
        }

        Webxx::dialog dialog_add_custom_column(event_context& ctx) {
            using namespace Webxx;
            return dialog{{_id{"dialog_add_custom_column"}, _class{"modal"}},
                dv{{_class{"modal-box w-3xl max-w-3xl"}},
                    form{{_method{"dialog"}},
                        button{{_class{"btn btn-sm btn-circle btn-ghost absolute right-2 top-2"}}, "x"}
                    },
                    h3{{_class{"text-lg font-bold"}}, "Add custom column"},
                    fieldset{{_class{"fieldset w-full"}},
                        label{{_class{"fieldset-label"}}, "Column name"},
                        ctx.on_input(input{{_id{"column_name"}, _class{"input w-full validator"}, _required{}, _placeholder{"Name"}}},
                            [this](webpp::event e){
                                auto name = *e.target().as<webpp::element>()->get_property<std::string>("value");
                                bool valid = !name.empty() && !table_custom_columns.contains(name);
                                if(valid) {
                                    webpp::get_element_by_id("column_add")->remove_class("btn-disabled");
                                    webpp::eval("document.getElementById('column_name').setCustomValidity('');");
                                } else {
                                    webpp::get_element_by_id("column_add")->add_class("btn-disabled");
                                    webpp::eval("document.getElementById('column_name').setCustomValidity('Column name must be non-empty and unique.');");
                                }
                            }
                        ),
                        dv{{_id{"column_name_validator"}, _class{"validator-hint"}}, "Column name must be non-empty and unique."},

                        label{{_class{"fieldset-label"}}, "Stencil"},
                        ctx.on_input(textarea{{_id{"column_stencil"}, _class{"textarea w-full min-h-[2.5rem]"}, _rows{"1"}, _placeholder{"Column stencil. Use {...} to insert values."}}},
                            [this](webpp::event) {
                                webpp::coro::submit([this]() -> webpp::coroutine<void> {
                                    co_await webpp::coro::next_tick();

                                    auto textarea = *webpp::get_element_by_id("column_stencil");
                                    auto validator = *webpp::get_element_by_id("column_stencil_validator");
                                    stencil_format = textarea["value"].as<std::string>().value_or("");
                                    if(auto r = common::stencil(stencil_format, *example_entry, stencil_functions)) {
                                        validator.inner_text(*r);
                                        validator.remove_class("text-error");
                                        validator.remove_class("font-bold");

                                        textarea.remove_class("textarea-error");
                                        textarea.add_class("textarea-success");
                                    } else {
                                        validator.inner_text(std::format("Stencil invalid: \"{}\"", r.error()));
                                        validator.add_class("text-error");
                                        validator.add_class("font-bold");

                                        textarea.remove_class("textarea-success");
                                        textarea.add_class("textarea-error");
                                    }
                                    co_return;
                                }());
                            }
                        ),

                        label{{_class{"fieldset-label"}}, "Preview"},
                        textarea{{_id{"column_stencil_validator"}, _class{"textarea w-full min-h-[2.5rem]"}, _rows{"1"}, _readonly{}}},

                        ctx.on_click(button{{_id{"column_add"}, _class{"btn btn-success mt-4 w-fit btn-disabled"}},
                            assets::icons::add, "Add column"},
                            [this](webpp::event e) {
                                auto el_name = *webpp::get_element_by_id("column_name");
                                auto el_stencil = *webpp::get_element_by_id("column_stencil");
                                auto el_validator = *webpp::get_element_by_id("column_stencil_validator");

                                auto name = *el_name.get_property<std::string>("value");
                                auto stencil = *el_stencil.get_property<std::string>("value");

                                webpp::eval("document.getElementById('dialog_add_custom_column').close();");
                                el_name.set_property("value", "");
                                webpp::eval("document.getElementById('column_name').setCustomValidity('');");

                                el_stencil.set_property("value", "");
                                el_stencil.remove_class("textarea-success");
                                el_stencil.remove_class("textarea-error");

                                el_validator.inner_text("");
                                el_validator.remove_class("text-error");
                                el_validator.remove_class("font-bold");
                                webpp::get_element_by_id("column_add")->add_class("btn-disabled");

                                table_custom_columns[name] = stencil;
                                auto json = glz::write_json(table_custom_columns).value_or("{}");
                                profile.set_data("table_custom_columns", json);

                                render_table();
                            }
                        ),
                    }
                },
                form{{_method{"dialog"}, _class{"modal-backdrop"}},
                    button{"close"}
                },
            };
        }

        Webxx::fragment render() override {
            static event_context ctx;
            ctx.clear();

            using namespace Webxx;
            return Webxx::fragment {
                dv{{_class{"flex flex-col m-0 p-0"}},
                    dv{{_class{"flex flex-col md:flex-row gap-4"}},
                        dv{{_id{"attributes"}, _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"attributes">("Select Attributes", attributes->attributes, selected_attributes, &profile, attributes->total_logs, true)},
                        dv{{_id{"resources"},  _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"resources">("Filter Resources", {}, selected_resources, &profile)},
                        dv{{_id{"scopes"},     _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"scopes">("Filter Scopes", scopes->scopes, selected_scopes, &profile, 1, false)},
                    },
                    dv{{_class{"flex flex-col md:flex-row gap-4 mt-4 justify-center"}},
                        ctx.on_click(button{{_class{"btn btn-primary"}},
                            span{{_id{"run_button_loading"}, _class{"loading loading-spinner hidden"}}},
                            span{{_id{"run_button_icon"}}, assets::icons::run},
                            "Query"
                        }, [this](webpp::event) {
                            webpp::get_element_by_id("run_button_icon")->add_class("hidden");
                            webpp::get_element_by_id("run_button_loading")->remove_class("hidden");
                            webpp::coro::submit(run_query());
                        }),
                        ctx.on_click(button{{_id{"reset_table_button"}, _class{"btn btn-secondary btn-disabled"}},
                            assets::icons::reset,
                            "Reset table"
                        }, [this](webpp::event e) {
                            table_column_order.clear();
                            render_table();
                        }),
                        button{{_id{"add_custom_column_button"}, _class{"btn btn-secondary"},
                                _onClick("document.getElementById('dialog_add_custom_column').showModal()")},
                            assets::icons::add,
                            "Add custom column"
                        },
                    },
                    dv{{_id{"table"}, _class{"mt-4 overflow-x-auto"}}},
                    dialog_add_custom_column(ctx),
                }
            };
        }
};

}
