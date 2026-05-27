export module frontend.pages:search;

import std;
import webpp;
import webxx;
import glaze;
import i18n;

import :page;
import :utils;

import common;

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

        Webxx::fragment render() override {
            static event_context ctx;
            ctx.clear();

            using namespace Webxx;
            return Webxx::fragment {
            };
        }
};

}
