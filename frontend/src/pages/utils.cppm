export module frontend.pages:utils;

import std;
import webpp;
import i18n;

import common;
import frontend.components;

using namespace mfk::i18n::literals;

namespace frontend::pages {

bool auto_select_resources(const common::logs_resources_response& resources,
    const components::selection_map& selections, components::selection_map& changes)
{
    for(const auto& [id, e] : resources.resources) {
        std::string key = std::to_string(id);
        if(!selections.contains(key) || !selections.at(key)) continue;

        const auto& [res, _] = e;
        auto name = res.guess_name();
        if(!name) continue;

        for(const auto& [id2, e2] : resources.resources) {
            const auto& [res2, _] = e2;
            if(id == id2) continue;
            auto name2 = res2.guess_name();
            if(!name2) continue;

            if(name == name2) {
                changes[std::to_string(id2)] = true;
            }
        }
    }
    return true;
}

template<typename T, typename Funcs>
void validate_stencil(webpp::element& textarea, webpp::element& validator, std::string_view stencil_format, const T& example_entry, Funcs& stencil_functions) {
    if(auto r = common::stencil(stencil_format, example_entry, stencil_functions)) {
        validator.inner_text(sanitize(*r));
        validator.remove_class("text-error");
        validator.remove_class("font-bold");

        textarea.remove_class("textarea-error");
        textarea.add_class("textarea-success");
    } else {
        validator.inner_text("Stencil invalid: \"{}\""_(sanitize(r.error())));
        validator.add_class("text-error");
        validator.add_class("font-bold");

        textarea.remove_class("textarea-success");
        textarea.add_class("textarea-error");
    }
}

std::string build_attributes_selector(const std::unordered_map<std::string, bool>& selected_attributes) {
    std::string attributes_selector{};
    for(const auto& [attr, selected] : selected_attributes) {
        if(selected) {
            attributes_selector += std::format("{},", attr);
        }
    }
    if(!attributes_selector.empty()) {
        attributes_selector.pop_back();
    }
    return attributes_selector;
}
std::string build_scopes_selector(const std::unordered_map<std::string, bool>& selected_scopes) {
    std::string scopes_selector{};
    for(const auto& [scope, selected] : selected_scopes) {
        if(selected) {
            scopes_selector += std::format("{},", scope.empty() ? "<empty>" : scope);
        }
    }
    return scopes_selector+"<dummy>";
}
std::string build_resources_selector(const std::unordered_map<std::string, bool>& selected_resources) {
    std::string resources_selector{};
    for(const auto& [res, selected] : selected_resources) {
        if(selected) {
            resources_selector += std::format("{},", res);
        }
    }
    return resources_selector+"0";
}

std::string resource_name(unsigned int id, const common::log_resource& resource) {
    return resource.guess_name().value_or("Resource #{}"_(id));
}

}
