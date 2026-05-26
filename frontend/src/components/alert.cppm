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
    export struct warning {
        constexpr static std::string_view class_name = "alert-warning";
        constexpr static std::string_view const* icon = &assets::icons::warning;
    };
    export struct success {
        constexpr static std::string_view class_name = "alert-success";
        constexpr static std::string_view const* icon = &assets::icons::success;
    };
};

export template<typename type = alert_types::error> auto alert(const std::string& id, std::string_view extra_classes = "") {
    using namespace Webxx;

    return dv{{_id{id}, _role{"alert"}, _class{std::format("alert alert-vertical sm:alert-horizontal hidden flex flex-col {} {}", type::class_name, extra_classes)}},
        dv{{_class{"flex flex-row items-center gap-2"}}, *type::icon, dv{{_id{std::format("{}_message", id)}}}},
        progress{{_id{std::format("{}_progress", id)}, _class{"progress hidden"}}}
    };
}
export void show_alert(std::string_view id, const std::string& title, const std::string& message, int progress = -1, int max = -1) {
    webpp::get_element_by_id(std::format("{}_message", id))->inner_html(std::format("<h3 class=\"font-bold\">{}</h3><div>{}</div>", title, message));
    webpp::get_element_by_id(id)->remove_class("hidden");
    if(progress != -1) {
        webpp::get_element_by_id(std::format("{}_progress", id))->remove_class("hidden");
        if(max != -1) {
            webpp::get_element_by_id(std::format("{}_progress", id))->set_property("value", progress);
            webpp::get_element_by_id(std::format("{}_progress", id))->set_property("max", max);
        } else {
            webpp::get_element_by_id(std::format("{}_progress", id))->remove_attribute("value");
            webpp::get_element_by_id(std::format("{}_progress", id))->remove_attribute("max");
        }
    } else {
        webpp::get_element_by_id(std::format("{}_progress", id))->add_class("hidden");
    }
}
export void hide_alert(std::string_view id) {
    webpp::get_element_by_id(id)->add_class("hidden");
}
export void show_alert(std::string id, const std::string& title, const std::string& message, std::chrono::milliseconds duration, int progress = -1, int max = -1) {
    show_alert(id, title, message, progress, max);
    webpp::set_timeout(duration, [id = std::move(id)](){
        hide_alert(id);
    });
}

}
