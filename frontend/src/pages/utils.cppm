export module frontend.pages:utils;

import std;
import webpp;
import i18n;
import glaze;

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
            attributes_selector += glz::url_encode(attr);
            attributes_selector += ",";
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
            scopes_selector += (scope.empty() ? "<empty>" : glz::url_encode(scope));
            scopes_selector += ",";
        }
    }
    return scopes_selector+"<dummy>";
}
std::string build_resources_selector(const std::unordered_map<std::string, bool>& selected_resources) {
    std::string resources_selector{};
    for(const auto& [res, selected] : selected_resources) {
        if(selected) {
            resources_selector += glz::url_encode(res);
            resources_selector += ",";
        }
    }
    return resources_selector+"0";
}

std::string_view log_severity_color(common::log_severity level) {
    switch(level) {
        case common::log_severity::UNSPECIFIED:
            return "neutral";
        case common::log_severity::TRACE:
        case common::log_severity::TRACE2:
        case common::log_severity::TRACE3:
        case common::log_severity::TRACE4:
            return "neutral";
        case common::log_severity::DEBUG:
        case common::log_severity::DEBUG2:
        case common::log_severity::DEBUG3:
        case common::log_severity::DEBUG4:
            return "info";
        case common::log_severity::INFO:
        case common::log_severity::INFO2:
        case common::log_severity::INFO3:
        case common::log_severity::INFO4:
            return "success";
        case common::log_severity::WARN:
        case common::log_severity::WARN2:
        case common::log_severity::WARN3:
        case common::log_severity::WARN4:
            return "warning";
        case common::log_severity::ERROR:
        case common::log_severity::ERROR2:
        case common::log_severity::ERROR3:
        case common::log_severity::ERROR4:
            return "error";
        case common::log_severity::FATAL:
        case common::log_severity::FATAL2:
        case common::log_severity::FATAL3:
        case common::log_severity::FATAL4:
            return "error";
    }
    return "neutral";
}

}
