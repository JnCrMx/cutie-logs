module backend.web;

namespace backend::web {

void Server::setupStaticRoutes() {
    router.get("/", [](const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
        response.send(Pistache::Http::Code::Ok, "Hello, World! :D");
        return Pistache::Rest::Route::Result::Ok;
    });
}

}
