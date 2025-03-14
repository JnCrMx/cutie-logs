module;
#include <string>
#include <string_view>

module backend.web;

import spdlog;

namespace backend::web {

struct entry {
    std::string_view path;
    std::string_view data;
    std::string_view type;
};

constexpr char style_css[] = {
    #embed "style.css"
};
constexpr char mainpage_html[] = {
    #embed "mainpage.html"
};
constexpr char mainpage_wasm[] = {
    #embed "mainpage.wasm"
};

constexpr std::array entries = {
    entry{"/style.css", {style_css, sizeof(style_css)}, "text/css"},
    entry{"/", {mainpage_html, sizeof(mainpage_html)}, "text/html"},
    entry{"/mainpage.wasm", {mainpage_wasm, sizeof(mainpage_wasm)}, "application/wasm"},
};

void Server::setupStaticRoutes() {
    for(auto& entry : entries) {
        auto& path = entry.path;
        auto& data = entry.data;
        auto& type = entry.type;

        router.get(
            std::string{path}, [data, type](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
                response.headers().add<Pistache::Http::Header::ContentType>(std::string{type});
                response.send(Pistache::Http::Code::Ok, data.data(), data.size());
                return Pistache::Rest::Route::Result::Ok;
            });
        logger->debug("Registered static route: {} -> {}", path, type);
    }
}

}
