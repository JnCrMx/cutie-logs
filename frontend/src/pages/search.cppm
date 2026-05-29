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

        Webxx::fragment render_subquery(common::search_query& v, common::body_query& q, event_context& ctx) {
            using namespace Webxx;

            return fragment{
                dv{{_class{"flex flex-row items-center gap-2 p-2 border rounded-xl w-fit"}},
                    dv{{_class{"font-bold"}}, "Body"_},
                    ctx.on_change(input{{_class{"input"}, _value{q.search}}}, [&](webpp::event e){
                        q.search = *e.target()["value"].as<std::string>();
                    })
                }
            };
        }
        Webxx::fragment render_subquery(common::search_query& v, common::attribute_query& q, event_context& ctx) {
            using namespace Webxx;

            return fragment{
                dv{{_class{"flex flex-row items-center gap-2 p-2 border rounded-xl w-fit"}},
                    dv{{_class{"font-bold"}}, "Attribute"_},
                    ctx.on_change(input{{_class{"input"}, _value{q.attribute}}}, [&](webpp::event e){
                        q.attribute = *e.target()["value"].as<std::string>();
                    }),
                    ctx.on_change(input{{_class{"input"}, _value{*q.search}}}, [&](webpp::event e){
                        q.search = *e.target()["value"].as<std::string>();
                    }),
                }
            };
        }

        Webxx::fragment render_compound(auto&& name, auto&& color, auto&& queries, event_context& ctx) {
            using namespace Webxx;

            return fragment{
                dv{{_class{"flex flex-row items-stretch gap-2"}},
                    dv{{_class{std::format("p-2 font-bold rounded-xl bg-{}-100 flex flex-col items-center justify-center", color)}},
                        dv{name}
                    },
                    dv{{_class{"flex flex-col gap-2"}},
                        [&](){
                            if constexpr (std::is_same_v<decltype(queries), common::search_query&>) {
                                return fragment{render_query(queries, ctx)};
                            } else {
                                return fragment{each(queries, [&](auto& sq){
                                    return render_query(sq, ctx);
                                })};
                            }
                        }()
                    }
                }
            };
        }
        Webxx::fragment render_subquery(common::search_query& v, common::and_query& q, event_context& ctx) {
            return render_compound("and"_, "orange", q.queries, ctx);
        }
        Webxx::fragment render_subquery(common::search_query& v, common::or_query& q, event_context& ctx) {
            return render_compound("or"_, "blue", q.queries, ctx);
        }
        Webxx::fragment render_subquery(common::search_query& v, common::detail::not_query& q, event_context& ctx) {
            return render_compound("not"_, "red", *q.query, ctx);
        }

        Webxx::dv render_query(common::search_query& v, event_context& ctx) {
            using namespace Webxx;

            return std::visit([&](auto&& q){
                return dv{{_id{std::format("search_query_{}", static_cast<void*>(&v))}},
                    render_subquery(v, q, ctx)
                };
            }, v);
        }

        Webxx::fragment render() override {
            static event_context ctx;
            ctx.clear();

            static common::search_query q = common::and_query{
                {
                    common::body_query{"test"},
                    common::not_query{
                        common::or_query{
                            {
                                common::body_query{"test2"},
                                common::attribute_query{"ClientHost", {common::search_type::text_contains}, "127.0.0.1"}
                            }
                        }
                    },
                    common::not_query{
                        common::and_query{
                            {
                                common::attribute_query{"ClientHost", {common::search_type::text_contains}, "127.0.0.1"},
                                common::not_query{
                                    common::body_query{"test2"}
                                }
                            }
                        }
                    }
                }
            };

            using namespace Webxx;
            return Webxx::fragment {
                dv{{_class{"flex flex-col m-0 p-0"}},
                    render_query(q, ctx),
                    ctx.on_click(button{"Test"}, [](webpp::event){
                        webpp::log("{}", *glz::write_json(q));
                    })
                }
            };
        }
};

}
