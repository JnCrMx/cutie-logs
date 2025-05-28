module;
#include <expected>
#include <memory>
#include <string>

module backend.notifications;

import :provider;
import cpr;
import glaze;
import common;
import spdlog;

namespace backend::notifications {

class webhook_provider : public provider {
    public:
        webhook_provider(spdlog::logger&, const glz::json_t& options, const glz::json_t& default_template) {
            if(!options.contains("url")) {
                throw error(error_code::invalid_options, "Webhook provider requires 'url' option");
            }
            m_url = options["url"].get<std::string>();
            if(options.contains("template")) {
                m_template = options["template"];
            } else {
                m_template = default_template;
            }

            if(options.contains("overrides")) {
                for(const auto& o : options["overrides"].get<glz::json_t::array_t>()) {
                    if(!glz::set(m_template, o["key"].get<std::string>(), o["value"])) {
                        throw error(error_code::invalid_options, "Failed to apply override: " + o["key"].get_string());
                    }
                }
            }
        }

        std::expected<void, error> notify(const common::alert_stencil_object& msg) override {
            glz::json_t payload = common::stencil_json(m_template, msg);
            auto json_payload = glz::write_json(payload);
            if(!json_payload) {
                return std::unexpected<error>(std::in_place, error_code::internal_error,
                    "Failed to serialize JSON payload: " + glz::format_error(json_payload.error()));
            }

            auto res = cpr::Post(cpr::Url{m_url}, cpr::Body(*json_payload), cpr::Header{{"Content-Type", "application/json"}});
            if(res.status_code == 0) {
                return std::unexpected<error>(std::in_place, error_code::internal_error,
                    "Failed to send notification: " + res.error.message);
            }
            if(res.status_code >= 400) {
                return std::unexpected<error>(std::in_place, error_code::internal_error,
                    "Webhook returned error: " + std::to_string(res.status_code) + " - " + res.status_line + ": " + res.text);
            }

            return std::expected<void, error>{};
        }
    private:
        std::string m_url;
        glz::json_t m_template;
};
auto webhook_factory(const glz::json_t& default_template) -> registry::func {
    return [default_template](spdlog::logger& logger, const glz::json_t& options) -> std::expected<std::unique_ptr<provider>, error> {
        try {
            return std::make_unique<webhook_provider>(logger, options, default_template);
        } catch (const error& e) {
            return std::unexpected<error>(e);
        } catch (const std::exception& e) {
            return std::unexpected<error>(std::in_place, error_code::internal_error, e.what());
        } catch (...) {
            return std::unexpected<error>(std::in_place, error_code::internal_error, "Unknown error occurred");
        }
    };
}

const auto discord_template = glz::json_t::object_t{
    {"content", glz::json_t::null_t{}},
    {"embeds", glz::json_t::array_t{
        glz::json_t::object_t{
            {"title", "Alert: {.severity}"},
            {"description", "{.body} ({.attributes})"},
            {"timestamp", "{.timestamp | from_timestamp | iso_8601}"},
            {"color", "{.severity | severity_color}!json"},
            {"author", glz::json_t::object_t{
                {"name", "{| resource_name}"}
            }},
            {"footer", glz::json_t::object_t{
                {"text", "{rule.name} • {rule.description}"}
            }},
            {"fields", glz::json_t::array_t{
                glz::json_t::object_t{
                    {"name", "Timestamp"},
                    {"value", "{.timestamp | from_timestamp | iso_date_time}"},
                    {"inline", true}
                },
                glz::json_t::object_t{
                    {"name", "Resource"},
                    {"value", "{| resource_name}"},
                    {"inline", true}
                },
                glz::json_t::object_t{
                    {"name", "Scope"},
                    {"value", "{.scope}"},
                    {"inline", true}
                },
                glz::json_t::object_t{
                    {"name", "Severity"},
                    {"value", "{.severity}"},
                    {"inline", true}
                },
            }}
        }
    }}
};
const common::notification_provider_info discord_info{
    .name = "Discord Webhook",
    .description = "Send notifications to a Discord channel via webhook.",
    .icon = "discord",
    .options = {
        {"url", "URL", "Webhook URL from Discord", common::notification_provider_option_type::STRING},
        {"template", "Template", "JSON template for the message payload", common::notification_provider_option_type::OBJECT, discord_template},
        {"overrides", "Overrides", "List of key-value pairs to override template values", common::notification_provider_option_type::ARRAY, glz::json_t::array_t{}}
    }
};
register_provider discord_reg("discord", webhook_factory(discord_template), discord_info);

}
