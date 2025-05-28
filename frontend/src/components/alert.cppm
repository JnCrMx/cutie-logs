export module frontend.components:alert;

import std;
import webxx;
import webpp;
import frontend.assets;

namespace frontend::components {

namespace alert_types {
    export struct error {
        constexpr static std::string_view class_name = "alert-error";
        constexpr static std::string_view const* icon = &assets::icons::error;
    };
};

export template<typename type = alert_types::error> auto alert(const std::string& id, std::string_view extra_classes = "") {
    using namespace Webxx;

    return dv{{_id{id}, _role{"alert"}, _class{std::string{"alert alert-vertical sm:alert-horizontal hidden "}+type::class_name+" "+extra_classes}},
        *type::icon, dv{{_id{std::format("{}_message", id)}}}
    };
}
export void show_alert(std::string_view id, const std::string& title, const std::string& message) {
    webpp::get_element_by_id(std::format("{}_message", id))->inner_html(std::format("<h3 class=\"font-bold\">{}</h3><div>{}</div>", title, message));
    webpp::get_element_by_id(id)->remove_class("hidden");
}
export void hide_alert(std::string_view id) {
    webpp::get_element_by_id(id)->add_class("hidden");
}

}
