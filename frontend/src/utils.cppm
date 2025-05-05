export module frontend.utils;

import std;
import webpp;

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

}
