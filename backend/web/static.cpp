module;
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

module backend.web;

import spdlog;

namespace backend::web {

struct entry {
    std::string_view path;
    std::string_view data;
    std::string_view type;
    std::string_view dev_path{};
};

constexpr char style_css[] = {
    #embed "style.css"
};
constexpr char mainpage_html[] = {
    #embed "mainpage.html"
};
constexpr char mainpage_wasm[] = {
    #ifdef NDEBUG
        #embed "mainpage.opt.wasm"
    #else
        #embed "mainpage.wasm"
    #endif
};

constexpr std::array entries = {
    entry{"/style.css", {style_css, sizeof(style_css)}, "text/css"},
    entry{"/", {mainpage_html, sizeof(mainpage_html)}, "text/html", "/mainpage.html"},
    entry{"/mainpage.wasm", {mainpage_wasm, sizeof(mainpage_wasm)}, "application/wasm"},
};

void Server::setup_static_routes() {
    for(auto& entry : entries) {
        auto path = entry.path;
        auto data = entry.data;
        auto type = entry.type;
        auto dev_path = entry.dev_path.empty() ? path : entry.dev_path;
        if(dev_path[0] == '/') {
            dev_path.remove_prefix(1);
        }

        router.get(
            std::string{path}, [this, data, type, dev_path](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
                response.headers().add<Pistache::Http::Header::ContentType>(std::string{type});
                if(static_dev_path.has_value()) {
                    auto path = *static_dev_path / dev_path;
                    if(std::filesystem::exists(path)) {
                        std::ifstream file(path, std::ios::binary);
                        if(file) {
                            std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            response.send(Pistache::Http::Code::Ok, data);
                            return Pistache::Rest::Route::Result::Ok;
                        }
                    }
                }
                response.send(Pistache::Http::Code::Ok, data.data(), data.size());
                return Pistache::Rest::Route::Result::Ok;
            });
        logger->debug("Registered static route: {} -> {}", path, type);
    }
}

}
