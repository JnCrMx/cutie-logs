module;

#include <expected>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>

export module backend.notifications:provider;
import glaze;

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

        virtual std::expected<void, error> notify() = 0;
};

export class registry {
public:
    static registry& instance() {
        static registry instance;
        return instance;
    }

    using func = std::add_pointer_t<std::expected<std::unique_ptr<provider>, error>(const glz::json_t& options)>;
    void register_provider(std::string&& name, func&& f) {
        m_providers.emplace(std::move(name), std::move(f));
    }

    std::expected<std::unique_ptr<provider>, error> create_provider(const std::string& name, const glz::json_t& options) {
        auto it = m_providers.find(name);
        if (it != m_providers.end()) {
            return it->second(options);
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

}
