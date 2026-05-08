#include <iostream>

import common;
import glaze;

template<auto options, typename T>
void test(T obj = {}) {
    auto serialized = glz::write<options>(obj);
    if(!serialized) {
        std::cerr << "[FAIL] " << glz::name_v<T> << " failed to serialize: " << glz::format_error(serialized.error()) << std::endl;
    } else {
        std::cout << " [OK]  " << glz::name_v<T> << " serialized successfully." << std::endl;
    }
    auto deserialized = glz::read<options, T>(*serialized);
    if(!deserialized) {
        std::cerr << "[FAIL] " << glz::name_v<T> << " failed to deserialize: " << glz::format_error(deserialized.error()) << std::endl;
    } else {
        std::cout << " [OK]  " << glz::name_v<T> << " deserialized successfully." << std::endl;
    }
}

template<auto options>
void test_all() {
    using namespace common;
    test<options, log_resource>();
    test<options, log_entry>();
    test<options, logs_response>();
    test<options, logs_attributes_response>();
    test<options, logs_scopes_response>();
    test<options, logs_resources_response>();
    test<options, standard_filters>();
    test<options, transform_action>();
    test<options, cleanup_rule>();
    test<options, cleanup_rules_response>();
    test<options, alert_rule>();
    test<options, alert_rules_response>();
    test<options, notification_provider_option>();
    test<options, notification_provider_info>();
    test<options, shared_settings>();
}

int main() {
    std::cout << "Testing JSON" << std::endl;
    test_all<common::json_opts>();

    std::cout << std::endl;

    std::cout << "Testing BEVE" << std::endl;
    test_all<common::beve_opts>();
}
