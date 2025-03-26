export module frontend.pages:table;

import std;
import web;
import web_coro;
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

        web::coro::coroutine<void> run_query() {
            std::string attributes_selector = build_attributes_selector(selected_attributes);
            std::string scopes_selector = build_scopes_selector(selected_scopes);
            std::string resources_selector = build_resources_selector(selected_resources);
            constexpr unsigned int limit = 100;

            auto url = std::format("/api/v1/logs?limit={}&attributes={}&scopes={}&resources={}",
                limit, attributes_selector, scopes_selector, resources_selector);
            auto logs =
                glz::read_beve<common::logs_response>(co_await web::coro::fetch(url))
                .value_or(common::logs_response{});

            web::remove_class("run_button_icon", "hidden");
            web::add_class("run_button_loading", "hidden");

            using namespace Webxx;
            std::set<std::string> attributes_set;
            for(const auto& [attr, selected] : selected_attributes) {
                if(selected) {
                    attributes_set.insert(attr);
                }
            }

            bool body = false;
            for(const auto& entry : logs.logs) {
                if(!entry.body.is_null()) {
                    body = true;
                    break;
                }
            }

            auto view = Webxx::table{{_class{"table table-pin-rows bg-base-200"}},
                thead{tr{
                    th{"Timestamp"},
                    th{"Scope"},
                    th{"Severity"},
                    each(attributes_set, [&](const std::string& attr) { return th{attr}; }),
                    maybe(body, [](){return th{"Body"};}),
                }},
                each(logs.logs, [&](const common::log_entry& entry) {
                    using sys_seconds_double = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>;
                    auto timestamp_double = sys_seconds_double{std::chrono::duration<double>{entry.timestamp}};
                    auto timestamp = std::chrono::time_point_cast<std::chrono::sys_seconds::duration>(timestamp_double);

                    auto attributes = entry.attributes.get_object();

                    return tr{
                        td{std::format("{:%Y-%m-%d %H:%M:%S}", timestamp)},
                        td{entry.scope},
                        td{common::log_severity_names[std::to_underlying(entry.severity)]},
                        each(attributes_set, [&](const std::string& attr) {
                            if(!attributes.contains(attr)) {
                                return td{"&lt;missing&gt;"};
                            }
                            return td{attributes.at(attr).is_string() ? attributes.at(attr).get_string() : attributes.at(attr).dump().value_or("error")};
                        }),
                        maybe(body, [&](){return td{entry.body.is_string() ? entry.body.get_string() : entry.body.dump().value_or("error")};}),
                    };
                })
            };
            web::set_html("table", Webxx::render(view));

            co_return;
        };

        void update_attributes() {
            std::transform(attributes->attributes.begin(), attributes->attributes.end(), std::inserter(selected_attributes, selected_attributes.end()),
                [](const auto& attr) { return std::pair{attr.first, false}; }); // all attributes are deselected by default
            web::set_html("attributes", Webxx::render(components::selection<"attributes">("Select Attributes", attributes->attributes, selected_attributes, attributes->total_logs, true)));
        }
        void update_scopes() {
            std::transform(scopes->scopes.begin(), scopes->scopes.end(), std::inserter(selected_scopes, selected_scopes.end()),
                [](const auto& scope) { return std::pair{scope.first, true}; }); // all scopes are selected by default
            web::set_html("scopes", Webxx::render(components::selection<"scopes">("Filter Scopes", scopes->scopes, selected_scopes, 1, false)));
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
            web::set_html("resources", Webxx::render(components::selection_detail<"resources">("Filter Resources", transformed_resources, selected_resources)));
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
                        }, [this](std::string_view) {
                            web::add_class("run_button_icon", "hidden");
                            web::remove_class("run_button_loading", "hidden");
                            web::coro::submit(run_query());
                        })
                    },
                    dv{{_id{"table"}, _class{"mt-4 overflow-x-auto"}}},
                }
            };
        }
};

}
