export module frontend.pages:search;

import std;
import webpp;
import webxx;
import glaze;
import i18n;

import :page;
import :utils;

import common;
import frontend.assets;
import frontend.components;
import frontend.utils;

using namespace mfk::i18n::literals;

namespace frontend::pages {

export class search : public page {
    private:
        profile_data& profile;

        r<common::logs_attributes_response>& attributes;
        r<common::logs_scopes_response>& scopes;
        r<common::logs_resources_response>& resources;
        r<common::attribute_index_response>& attribute_indices;
    public:
        search(profile_data& profile,
               r<common::logs_attributes_response>& attributes, r<common::logs_scopes_response>& scopes, r<common::logs_resources_response>& resources,
               r<common::attribute_index_response>& attribute_indices)
            : profile(profile), attributes{attributes}, scopes{scopes}, resources{resources}, attribute_indices{attribute_indices}
        {
            profile.add_callback([this](auto&) { if(is_open) { open(); }});
        }

        void open() override {
            page::open();
        }

        struct parent_functions {
            std::function<void(event_context&)> rerender;
            std::function<void(common::search_query& sq)> remove_child;
        };

        static Webxx::fragment render_query_actions(event_context& ctx, common::search_query& sq, parent_functions parent) {
            using namespace Webxx;

            return fragment{
                maybe(parent.remove_child, [&](){
                    return ctx.on_click(button{assets::icons::remove}, [&ctx, &sq, parent](webpp::event) {
                        parent.remove_child(sq);
                        parent.rerender(ctx);
                    });
                }),
            };
        }

        static void rerender_subquery(common::search_query& v, event_context& ctx, parent_functions parent) {
            std::string id = std::format("search_query_{}", static_cast<void*>(&v));
            auto el = webpp::get_element_by_id(id);
            if(!el) {
                return;
            }
            el->inner_html(Webxx::render(render_query_inner(v, ctx, parent)));
        }

        static Webxx::fragment render_subquery(common::search_query& v, common::body_query& q, event_context& ctx, parent_functions parent) {
            using namespace Webxx;

            return fragment{
                dv{{_class{"flex flex-row items-center gap-2 p-2 bg-gray-300 rounded-xl w-fit"}},
                    render_query_actions(ctx, v, parent),
                    dv{{_class{"font-bold"}}, "Body"_},
                    ctx.on_change(input{{_class{"input"}, _placeholder{"Search text"_}, _value{q.search}}}, [&](webpp::event e){
                        q.search = *e.target()["value"].as<std::string>();
                    })
                }
            };
        }
        static Webxx::fragment render_subquery(common::search_query& v, common::attribute_query& q, event_context& ctx, parent_functions parent) {
            using namespace Webxx;

            return fragment{
                dv{{_class{"flex flex-row items-center gap-2 p-2 bg-gray-300 rounded-xl w-fit"}},
                    render_query_actions(ctx, v, parent),
                    dv{{_class{"font-bold"}}, "Attribute"_},
                    ctx.on_change(input{{_class{"input"}, _placeholder{"Attribute name"_}, _value{q.attribute}}}, [&](webpp::event e){
                        q.attribute = *e.target()["value"].as<std::string>();
                    }),
                    ctx.on_change(input{{_class{"input"}, _placeholder{"Search text"_}, _value{*q.search}}}, [&](webpp::event e){
                        q.search = *e.target()["value"].as<std::string>();
                    }),
                }
            };
        }

        static Webxx::fragment render_compound(common::search_query& v, auto&& name, auto&& color, auto&& queries, event_context& ctx, parent_functions parent,
            auto&& remove_child, auto&& transmute, auto&& add_child)
        {
            using namespace Webxx;

            parent_functions this_functions = {
                .rerender = [&v, parent](event_context& ctx){
                    rerender_subquery(v, ctx, parent);
                },
                .remove_child = remove_child
            };

            return fragment{
                dv{{_class{"flex flex-row items-stretch gap-2"}},
                    dv{{_class{std::format("w-16 p-2 font-bold bg-{}-300 rounded-xl flex flex-col items-center justify-center", color)}},
                        dv{{_class{"mt-auto"}}, name},
                        dv{{_class{"mt-auto flex flex-row"}},
                            [&](){
                                if constexpr (std::is_same_v<decltype(transmute), std::nullptr_t&&>) {
                                    return fragment{};
                                } else {
                                    return ctx.on_click(button{assets::icons::edit}, [transmute, &v, &ctx, parent](webpp::event){
                                        transmute();
                                        rerender_subquery(v, ctx, parent);
                                    });
                                }
                            }(),
                            render_query_actions(ctx, v, parent),
                        }
                    },
                    dv{{_class{"flex flex-col gap-2"}},
                        [&, parent](){
                            if constexpr (std::is_same_v<decltype(queries), std::nullptr_t&&>) {
                                return fragment{};
                            } else if constexpr (std::is_same_v<decltype(queries), common::search_query&>) {
                                return fragment{render_query(queries, ctx, this_functions)};
                            } else {
                                return fragment{each(queries, [&, parent](auto& sq){
                                    return render_query(sq, ctx, this_functions);
                                })};
                            }
                        }(),
                        [&](){
                            if constexpr (std::is_same_v<decltype(add_child), std::nullptr_t&&>) {
                                return fragment{};
                            } else {
                                auto on_click = [add_child, &v, &ctx, parent](common::search_query&& sq, webpp::event){
                                    add_child(std::move(sq));
                                    rerender_subquery(v, ctx, parent);
                                };
                                return fragment{
                                    dv{{_class{"dropdown"}},
                                        dv{{_class{"btn"}, _tabindex{"0"}, _role{"button"}}, assets::icons::add},
                                        ul{{_class{"dropdown-content menu bg-base-100 rounded-box z-1 w-52 p-2 shadow-sm"}, _tabindex{"-1"}},
                                            li{ctx.on_click(a{"Body"_}, std::bind_front(on_click, common::body_query{}))},
                                            li{ctx.on_click(a{"Attribute"_}, std::bind_front(on_click, common::attribute_query{.types={common::search_type::fulltext, common::search_type::text_contains}, .search=""}))},
                                            li{ctx.on_click(a{"and"_}, std::bind_front(on_click, common::and_query{}))},
                                            li{ctx.on_click(a{"or"_}, std::bind_front(on_click, common::or_query{}))},
                                            li{ctx.on_click(a{"not"_}, std::bind_front(on_click, common::not_query{}))},
                                        }
                                    }
                                };
                            }
                        }()
                    }
                }
            };
        }
        static Webxx::fragment render_subquery(common::search_query& v, common::and_query& q, event_context& ctx, parent_functions parent) {
            auto remove_child = [&q](common::search_query& sq){
                auto it = std::find_if(q.queries.begin(), q.queries.end(), [&sq](auto& q){
                    return &sq == &q;
                });
                if(it != q.queries.end()) {
                    q.queries.erase(it);
                }
            };
            auto transmute = [&v, &q, parent](){
                std::vector<common::search_query> queries = std::move(q.queries);
                v = common::or_query{std::move(queries)};
            };
            auto add_child = [&q](common::search_query&& sq){
                q.queries.push_back(std::move(sq));
            };
            return render_compound(v, "and"_, "orange", q.queries, ctx, parent, std::move(remove_child), std::move(transmute), std::move(add_child));
        }
        static Webxx::fragment render_subquery(common::search_query& v, common::or_query& q, event_context& ctx, parent_functions parent) {
            auto remove_child = [&q](common::search_query& sq){
                auto it = std::find_if(q.queries.begin(), q.queries.end(), [&sq](auto& q){
                    return &sq == &q;
                });
                if(it != q.queries.end()) {
                    q.queries.erase(it);
                }
            };
            auto transmute = [&v, &q, parent](){
                std::vector<common::search_query> queries = std::move(q.queries);
                v = common::and_query{std::move(queries)};
            };
            auto add_child = [&q](common::search_query&& sq){
                q.queries.push_back(std::move(sq));
            };
            return render_compound(v, "or"_, "blue", q.queries, ctx, parent, std::move(remove_child), std::move(transmute), std::move(add_child));
        }
        static Webxx::fragment render_subquery(common::search_query& v, common::detail::not_query& q, event_context& ctx, parent_functions parent) {
            auto remove_child = [&q](common::search_query& sq){
                q.query = nullptr;
            };
            auto add_child = [&q](common::search_query&& sq){
                q.query = std::make_shared<common::search_query>(std::move(sq));
            };
            if(q.query) {
                return render_compound(v, "not"_, "red", *q.query, ctx, parent, std::move(remove_child), nullptr, nullptr);
            } else {
                return render_compound(v, "not"_, "red", nullptr, ctx, parent, std::move(remove_child), nullptr, std::move(add_child));
            }
        }

        static Webxx::fragment render_query_inner(common::search_query& v, event_context& ctx, parent_functions parent) {
            using namespace Webxx;

            return std::visit([&](auto&& q){
                return render_subquery(v, q, ctx, parent);
            }, v);
        }

        static Webxx::dv render_query(common::search_query& v, event_context& ctx, parent_functions parent) {
            using namespace Webxx;

            return dv{{_id{std::format("search_query_{}", static_cast<void*>(&v))}},
                render_query_inner(v, ctx, parent)
            };
        }

        common::search_query query = common::or_query{{common::body_query{}}};

        void finish_search() {
            webpp::get_element_by_id("search_button")->remove_class("btn-disabled");
            webpp::get_element_by_id("search_button_icon")->remove_class("hidden");
            webpp::get_element_by_id("search_button_loading")->add_class("hidden");
        }

        auto do_search() -> webpp::coroutine<void> {
            common::scope_exit guard{[this](){ finish_search(); }};

            auto beve = glz::write<common::beve_opts>(query);
            if(beve) {
                components::show_alert("alert_error", std::string{"Failed to serialize search query"_}, glz::format_error(beve));
                co_return;
            }

            webpp::js_object request = webpp::js_object::create();
            request["headers"] = utils::fetch_headers;
            request["body"] = webpp::uint8array::create(*beve);
            request["method"] = "POST";

            webpp::response res = co_await webpp::coro::fetch("/api/v1/search", request);
            if(!res.ok()) {
                // TODO: show error
                co_return;
            }
            auto bytes = co_await res.co_bytes();
            common::logs_response result;
            if(auto error = glz::read<common::beve_opts>(result, bytes)) {
                // TODO: show error
                co_return;
            }

            // TODO: render search results
            co_return;
        }

        Webxx::fragment render() override {
            static event_context ctx;
            ctx.clear();

            static parent_functions top_funcs = {
                .rerender = {},
                .remove_child = {},
            };

            using namespace Webxx;
            return fragment {
                dv{{_class{"flex flex-col m-0 p-0 items-center gap-4"}},
                    dv{{_class{"self-start w-full"}},
                        render_query(query, ctx, top_funcs),
                    },
                    ctx.on_click(button{{_id{"search_button"}, _class{"btn btn-primary btn-wide"}},
                        span{{_id{"search_button_loading"}, _class{"loading loading-spinner hidden"}}},
                        span{{_id{"search_button_icon"}}, assets::icons::run},
                        "Search"_
                    }, [this](webpp::event){
                        webpp::get_element_by_id("search_button")->add_class("btn-disabled");
                        webpp::get_element_by_id("search_button_icon")->add_class("hidden");
                        webpp::get_element_by_id("search_button_loading")->remove_class("hidden");
                        components::hide_alert("alert_error");
                        webpp::coro::submit(do_search());
                    }),
                    components::alert("alert_error", "w-[50%] items-start"),
                    dv{{_id{"search_results"}, _class{"self-start w-full flex flex-col gap-2 items-stretch overflow-y-auto h-full"}},}
                }
            };
        }
};

}
