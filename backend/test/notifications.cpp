#include <iostream>

import backend.notifications;
import common;

import spdlog;
import glaze;

int main(int argc, const char** argv) {
    if(argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <discord webhook url>" << std::endl;
        return 2;
    }

    glz::generic options = {
        {"url", std::string{argv[1]}}
    };

    auto& registry = backend::notifications::registry::instance();
    auto provider = registry.create_provider("discord", *spdlog::default_logger(), options).value();

    common::alert_rule rule{

    };
    common::log_entry log{

    };
    common::log_resource resource{

    };
    common::alert_stencil_object alert{&rule, &resource, &log};
    if(auto res = provider->notify(alert); !res) {
        spdlog::error("Failed to send notification: {}", res.error().message);
        return 1;
    }
    return 0;
}
