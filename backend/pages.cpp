module backend;

import pistache;

namespace backend {
    void setup_static_pages(Pistache::Rest::Router& router) {
        router.get("/", [](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
            response.send(Pistache::Http::Code::Ok, "Hello, World! :D");
            return Pistache::Rest::Route::Result::Ok;
        });
        router.get("/mainpage.wasm", [](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
            const char data[] = {
                #embed "mainpage.wasm"
            };
            response.send(Pistache::Http::Code::Ok, data);
            return Pistache::Rest::Route::Result::Ok;
        });
    }
}
