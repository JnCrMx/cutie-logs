module;
#include <expected>
#include <memory>
#include <string>

#include <netinet/in.h>
#include <curl/curl.h>

module backend.notifications;

import :provider;
import cpr;
import glaze;
import common;
import spdlog;
import backend.utils;

namespace backend::notifications {

class webhook_provider : public provider {
    private:
        struct curl_opensocket_data_t {
            NetworkIpFilter* ipFilter;
            spdlog::logger& logger;
        };
        static curl_socket_t curl_opensocket_function(void* clientp, curlsocktype purpose, curl_sockaddr* address) {
            if(purpose == CURLSOCKTYPE_IPCXN) {
                bool accept = static_cast<curl_opensocket_data_t*>(clientp)->ipFilter->filter(&address->addr);
                if(!accept) {
                    std::string ip_string = "invalid IP address";
                    if(address->family == AF_INET) {
                        auto* sockaddr = reinterpret_cast<sockaddr_in*>(&address->addr);
                        uint32_t ip = ntohl(sockaddr->sin_addr.s_addr);
                        ip_string = common::ipv4_to_string(ip);
                    }
                    static_cast<curl_opensocket_data_t*>(clientp)->logger.warn("Blocking webhook request to IP address \"{}\" due to network IP filter rules", ip_string);
                    return CURL_SOCKET_BAD;
                }
            }
            return socket(address->family, address->socktype, address->protocol);
        }
    public:
        webhook_provider(spdlog::logger&, const glz::generic& options, const glz::generic& default_template) {
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
                for(const auto& o : options["overrides"].get<glz::generic::array_t>()) {
                    if(!glz::set(m_template, o["key"].get<std::string>(), o["value"])) {
                        throw error(error_code::invalid_options, "Failed to apply override: " + o["key"].get_string());
                    }
                }
            }
        }

        std::expected<void, error> notify(spdlog::logger& logger, const common::alert_stencil_object& msg, NetworkIpFilter* ipFilter) override {
            glz::generic payload = common::stencil_json(m_template, msg);
            auto json_payload = glz::write_json(payload);
            if(!json_payload) {
                return std::unexpected<error>(std::in_place, error_code::internal_error,
                    "Failed to serialize JSON payload: " + glz::format_error(json_payload.error()));
            }

            curl_opensocket_data_t curl_opensocket_data{ipFilter, logger};

            cpr::Session session{};
            session.SetUrl(cpr::Url{m_url});
            session.SetBody(cpr::Body{std::move(*json_payload)});
            session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
            if(ipFilter) {
                curl_easy_setopt(session.GetCurlHolder()->handle, CURLOPT_OPENSOCKETFUNCTION, curl_opensocket_function);
                curl_easy_setopt(session.GetCurlHolder()->handle, CURLOPT_OPENSOCKETDATA, &curl_opensocket_data);
            }

            auto res = session.Post();
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
        glz::generic m_template;
};
auto webhook_factory(const glz::generic& default_template) -> registry::func {
    return [default_template](spdlog::logger& logger, const glz::generic& options) -> std::expected<std::unique_ptr<provider>, error> {
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

const auto discord_template = glz::generic::object_t{
    {"content", glz::generic::null_t{}},
    {"embeds", glz::generic::array_t{
        glz::generic::object_t{
            {"title", "Alert: {.severity}"},
            {"description", "{.body} ({.attributes})"},
            {"timestamp", "{.timestamp | from_timestamp | iso_8601}"},
            {"color", "{.severity | severity_color}!json"},
            {"author", glz::generic::object_t{
                {"name", "{| resource_name}"}
            }},
            {"footer", glz::generic::object_t{
                {"text", "{rule.name} • {rule.description}"}
            }},
            {"fields", glz::generic::array_t{
                glz::generic::object_t{
                    {"name", "Timestamp"},
                    {"value", "{.timestamp | from_timestamp | iso_date_time}"},
                    {"inline", true}
                },
                glz::generic::object_t{
                    {"name", "Resource"},
                    {"value", "{| resource_name}"},
                    {"inline", true}
                },
                glz::generic::object_t{
                    {"name", "Scope"},
                    {"value", "{.scope}"},
                    {"inline", true}
                },
                glz::generic::object_t{
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
        {"overrides", "Overrides", "List of key-value pairs to override template values", common::notification_provider_option_type::ARRAY, glz::generic::array_t{}}
    }
};
register_provider discord_reg("discord", webhook_factory(discord_template), discord_info);

}
