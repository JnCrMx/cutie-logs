export module frontend.utils;

import std;
import webpp;

namespace frontend::utils {

const webpp::js_object fetch_headers = [](){
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

export std::string format_duration(std::chrono::seconds duration) {
    if(duration <= std::chrono::seconds{0}) {
        return "0 seconds";
    }
    if(duration < std::chrono::minutes{1}) {
        return std::format("{} seconds", duration.count());
    }
    if(duration < std::chrono::hours{1}) {
        return std::format("{:.2f} minutes", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::minutes::period>>(duration).count());
    }
    if(duration < std::chrono::hours{24}) {
        return std::format("{:.2f} hours", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::hours::period>>(duration).count());
    }
    return std::format("{:.2f} days", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::hours::period>>(duration).count() / 24);
}

}
