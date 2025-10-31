module;
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <span>

module backend.web;

import spdlog;

namespace backend::web {

constexpr char git_log[] = {
#embed ".git/logs/HEAD"
};
namespace detail {
    template<std::array src, std::size_t start, std::size_t length>
    constexpr auto substr() {
        std::array<char, length> result{};
        std::copy(src.begin() + start, src.begin() + start + length, result.begin());
        return result;
    }
    template<std::array a, std::array b>
    constexpr auto concat() {
        std::array<typename decltype(a)::value_type, a.size() + b.size()> result{};
        std::copy(a.begin(), a.end(), result.begin());
        std::copy(b.begin(), b.end(), result.begin() + a.size());
        return result;
    }

    template<typename T, std::size_t N>
    constexpr auto str_to_array(const T (&arr)[N]) {
        std::array<T, N - 1> result{};
        std::copy(arr, arr + N - 1, result.begin());
        return result;
    }
};
constexpr auto git_commit_hash = [](){
    constexpr std::string_view sv{git_log, sizeof(git_log)};
    constexpr auto pos1 = sv.find_last_of('\n');
    constexpr auto pos2 = sv.find_last_of('\n', pos1-1);
    constexpr auto pos3 = pos2 == std::string_view::npos ? 0 : pos2; // the file might have only one line
    constexpr auto pos4 = sv.find(' ', pos3);

    return detail::substr<std::to_array(git_log), pos4+1, 40>();
}();

#ifdef NDEBUG
constexpr char etag_prefix[] = "release_";
#else
constexpr char etag_prefix[] = "debug_";
#endif
constexpr auto etag_array = detail::concat<detail::str_to_array(etag_prefix), git_commit_hash>();
constexpr auto etag = std::string_view{etag_array};

struct entry {
    std::string_view path;
    std::span<const unsigned char> data;
    std::string_view type;
    std::string_view dev_path{};
};

constexpr unsigned char style_css[] = {
    #embed "style.css"
};
constexpr unsigned char mainpage_html[] = {
    #embed "mainpage.html"
};
constexpr unsigned char mainpage_wasm[] = {
    #ifdef NDEBUG
        #embed "mainpage.opt.wasm"
    #else
        #embed "mainpage.wasm"
    #endif
};
constexpr unsigned char webpp_js[] = {
    #embed "webpp.js"
};

constexpr std::array entries = {
    entry{"/style.css", {style_css, sizeof(style_css)}, "text/css"},
    entry{"/", {mainpage_html, sizeof(mainpage_html)}, "text/html", "/mainpage.html"},
    entry{"/mainpage.wasm", {mainpage_wasm, sizeof(mainpage_wasm)}, "application/wasm"},
    entry{"/webpp.js", {webpp_js, sizeof(webpp_js)}, "application/javascript"},
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
                if(request.headers().has<Pistache::Http::Header::IfNoneMatch>()) {
                    auto if_none_match = request.headers().get<Pistache::Http::Header::IfNoneMatch>();
                    if(!if_none_match->test(etag)) {
                        response.send(Pistache::Http::Code::Not_Modified);
                        return Pistache::Rest::Route::Result::Ok;
                    }
                }

                response.headers().add<Pistache::Http::Header::ETag>(std::string{etag});
                response.send(Pistache::Http::Code::Ok, reinterpret_cast<const char*>(data.data()), data.size());
                return Pistache::Rest::Route::Result::Ok;
            });
        logger->debug("Registered static route: {} -> {}", path, type);
    }
}

}
