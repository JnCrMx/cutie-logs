module;

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>

export module backend.notifications:provider;
import glaze;
import common;
import spdlog;

namespace backend::notifications {

export enum class error_code {
    not_found,
    invalid_options,
    not_supported,
    internal_error,
    not_available,
};
export struct error {
    error_code code;
    std::string message;

    error(error_code code, std::string message) : code(code), message(std::move(message)) {}
};

export class provider {
    public:
        provider() = default;
        virtual ~provider() = default;

        virtual std::expected<void, error> notify(const common::alert_stencil_object& msg) = 0;
};

export class registry {
public:
    static registry& instance() {
        static registry instance;
        return instance;
    }

    using func = std::function<std::expected<std::unique_ptr<provider>, error>(spdlog::logger& logger, const glz::json_t& options)>;
    void register_provider(std::string&& name, func&& f) {
        m_providers.emplace(std::move(name), std::move(f));
    }

    std::expected<std::unique_ptr<provider>, error> create_provider(const std::string& name, spdlog::logger& logger, const glz::json_t& options) {
        auto it = m_providers.find(name);
        if (it != m_providers.end()) {
            return it->second(logger, options);
        }
        return std::unexpected<error>(std::in_place, error_code::not_found, "Provider not found: " + name);
    }
private:
    registry() = default;
    ~registry() = default;
    registry(const registry&) = delete;
    registry& operator=(const registry&) = delete;
    registry(registry&&) = delete;
    registry& operator=(registry&&) = delete;

    std::unordered_map<std::string, func> m_providers;
};

export class register_provider {
    public:
        register_provider(std::string name, registry::func f) {
            registry::instance().register_provider(std::move(name), std::move(f));
        }
};
export template<typename T> auto default_provider_factory = [](spdlog::logger& logger, const glz::json_t& options) -> std::expected<std::unique_ptr<provider>, error> {
    static_assert(std::is_base_of_v<provider, T>, "T must be derived from provider");
    try {
        return std::make_unique<T>(logger, options);
    } catch (const error& e) {
        return std::unexpected<error>(e);
    } catch (const std::exception& e) {
        return std::unexpected<error>(std::in_place, error_code::internal_error, e.what());
    } catch (...) {
        return std::unexpected<error>(std::in_place, error_code::internal_error, "Unknown error occurred");
    }
};

}
