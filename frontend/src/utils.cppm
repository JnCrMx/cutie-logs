export module frontend.utils;

import std;
import i18n;
import webpp;

import common;

namespace frontend::utils {

export const webpp::js_object fetch_headers = [](){
    webpp::js_object obj = webpp::js_object::create();
    obj.set_property("Accept", "application/prs.beve");
    obj.set_property("Content-Type", "application/prs.beve");
    return obj;
}();
export const webpp::js_object fetch_options = [](){
    webpp::js_object obj = webpp::js_object::create();
    obj.set_property("headers", fetch_headers);
    return obj;
}();

export std::string resource_name(const common::log_resource& resource) {
    using namespace mfk::i18n::literals;
    return resource.guess_name().value_or("Resource #{}"_(resource.id));
}

export std::string sanitize(std::string_view sv) {
    std::string out{};
    out.reserve(sv.size());
    for(auto c : sv) {
        switch(c) {
            case '<':
                out.append("&lt;");
                break;
            case '>':
                out.append("&gt;");
                break;
            case '&':
                out.append("&amp;");
                break;
            default:
                out.append({c});
        }
    }
    return out;
}

}
