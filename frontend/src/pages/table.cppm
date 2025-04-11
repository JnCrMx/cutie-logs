export module frontend.pages:table;

import std;
import webpp;
import webxx;
import glaze;

import :page;

import common;
import frontend.assets;
import frontend.components;

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

        Webxx::fragment make_table() {
            using namespace Webxx;
            std::set<std::string> attributes_set;
            for(const auto& [attr, selected] : selected_attributes) {
                if(selected) {
                    attributes_set.insert(attr);
                }
            }

            if(table_column_order.empty()) {
                table_column_order = {":Timestamp", ":Resource", ":Scope", ":Severity"};
            }
            for(auto& a : attributes_set) {
                if(std::find(table_column_order.begin(), table_column_order.end(), a) == table_column_order.end()) {
                    table_column_order.push_back(a);
                }
            }

            for(const auto& entry : logs.logs) {
                if(!entry.body.is_null() && (!entry.body.is_string() || !entry.body.get_string().empty())) {
                    if(std::find(table_column_order.begin(), table_column_order.end(), "body") == table_column_order.end()) {
                        table_column_order.push_back(":Body");
                    }
                    break;
                }
            }

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
                webpp::get_element_by_id("reset_table_button")->remove_class("btn-disabled");
            };

            unsigned int column_pos = 0;
            auto make_th = [&](std::string text) {
                constexpr static char dataColumnPosAttr[] = "data-column-pos";
                using _dataColumnPos = Webxx::attr<dataColumnPosAttr>;

                if(text.starts_with(':')) {
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

                            if(!attributes.contains(attr)) {
                                return td{"&lt;missing&gt;"};
                            }
                            return td{attributes.at(attr).is_string() ? attributes.at(attr).get_string() : attributes.at(attr).dump().value_or("error")};
                        })
                    };
                })
            };
            return fragment{view};
        }
        void render_table() {
            webpp::get_element_by_id("table")->inner_html(Webxx::render(make_table()));
        }

        webpp::coroutine<void> run_query() {
            std::string attributes_selector = build_attributes_selector(selected_attributes);
            std::string scopes_selector = build_scopes_selector(selected_scopes);
            std::string resources_selector = build_resources_selector(selected_resources);
            constexpr unsigned int limit = 100;

            auto url = std::format("/api/v1/logs?limit={}&attributes={}&scopes={}&resources={}",
                limit, attributes_selector, scopes_selector, resources_selector);
            logs =
                glz::read_beve<common::logs_response>(co_await webpp::coro::fetch(url).then(std::mem_fn(&webpp::response::co_bytes)))
                .value_or(common::logs_response{});

            webpp::get_element_by_id("run_button_icon")->remove_class("hidden");
            webpp::get_element_by_id("run_button_loading")->add_class("hidden");

            render_table();

            co_return;
        };

        void update_attributes() {
            std::transform(attributes->attributes.begin(), attributes->attributes.end(), std::inserter(selected_attributes, selected_attributes.end()),
                [](const auto& attr) { return std::pair{attr.first, false}; }); // all attributes are deselected by default
            webpp::get_element_by_id("attributes")->inner_html(Webxx::render(
                components::selection<"attributes">("Select Attributes",
                    attributes->attributes, selected_attributes, attributes->total_logs, true)));
        }
        void update_scopes() {
            std::transform(scopes->scopes.begin(), scopes->scopes.end(), std::inserter(selected_scopes, selected_scopes.end()),
                [](const auto& scope) { return std::pair{scope.first, true}; }); // all scopes are selected by default
            webpp::get_element_by_id("scopes")->inner_html(Webxx::render(
                components::selection<"scopes">("Filter Scopes", scopes->scopes, selected_scopes, 1, false)));
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
            std::transform(transformed_resources.begin(), transformed_resources.end(), std::inserter(selected_resources, selected_resources.end()),
                [](const auto& res) { return std::pair{res.first, false}; }); // all resources are deselected by default
            webpp::get_element_by_id("resources")->inner_html(Webxx::render(
                components::selection_detail<"resources">("Filter Resources", transformed_resources, selected_resources, 1, false, "resource")));
        }

        r<common::logs_attributes_response>& attributes;
        r<common::logs_scopes_response>& scopes;
        r<common::logs_resources_response>& resources;

        std::string stencil_format;
        std::unordered_map<std::string, bool> selected_attributes, selected_resources, selected_scopes;
        std::unordered_map<std::string, std::tuple<std::string, unsigned int>> transformed_resources;
    public:
        table(r<common::logs_attributes_response>& attributes, r<common::logs_scopes_response>& scopes, r<common::logs_resources_response>& resources)
            : attributes{attributes}, scopes{scopes}, resources{resources}
        {
            attributes.add_callback([this](auto&) { if(is_open) update_attributes(); });
            scopes.add_callback([this](auto&) { if(is_open) update_scopes(); });
            resources.add_callback([this](auto&) { if(is_open) update_resources(); });
        }

        void open() override {
            page::open();

            update_attributes();
            update_scopes();
            update_resources();
        }

        Webxx::fragment render() override {
            static event_context ctx;
            ctx.clear();

            using namespace Webxx;
            return Webxx::fragment {
                dv{{_class{"flex flex-col m-0 p-0"}},
                    dv{{_class{"flex flex-col md:flex-row gap-4"}},
                        dv{{_id{"attributes"}, _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"attributes">("Select Attributes", attributes->attributes, selected_attributes, attributes->total_logs, true)},
                        dv{{_id{"resources"},  _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"resources">("Filter Resources", {}, selected_resources)},
                        dv{{_id{"scopes"},     _class{"md:basis-0 md:grow *:max-h-60"}}, components::selection<"scopes">("Filter Scopes", scopes->scopes, selected_scopes, 1, false)},
                    },
                    dv{{_class{"flex flex-row gap-4 mt-4 justify-center"}},
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
                            e.target().as<webpp::element>()->add_class("btn-disabled");
                        }),
                    },
                    dv{{_id{"table"}, _class{"mt-4 overflow-x-auto"}}},
                }
            };
        }
};

}
