module;
#include <expected>
#include <format>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module common:stencil;

import glaze;
import :structs;

namespace common {
    template<typename T>
    std::expected<std::string, std::string> format_if_possible(const T& obj) {
        if constexpr (std::formattable<T, char>) {
            return std::format("{}", obj);
        } else if constexpr (std::is_same_v<T, glz::json_t>) {
            if(obj.is_string()) {
                return obj.get_string();
            } else if(obj.is_number()) {
                return format_if_possible(obj.get_number());
            } else if(obj.is_boolean()) {
                return obj.get_boolean() ? "true" : "false";
            } else {
                return obj.dump().transform_error([](auto&& err) {
                    return glz::format_error(err);
                });
            }
        } else if constexpr (std::is_same_v<T, common::log_severity>) {
            return common::log_severity_names[std::to_underlying(obj)];
        } else {
            return std::unexpected{"Cannot format type: "+std::string(glz::name_v<T>)};
        }
    }

    template<typename T>
    concept can_get_field = glz::reflectable<T> || std::same_as<T, glz::json_t>;

    template<can_get_field T>
    auto get_keys(const T& obj) {
        if constexpr (glz::reflectable<T>) {
            return glz::reflect<T>::keys;
        } else {
            if(obj.is_object()) {
                const glz::json_t::object_t& o = obj.get_object();
                std::vector<std::string_view> keys{};
                keys.reserve(o.size());
                for(auto& [key, _] : o) {
                    keys.push_back(key);
                }
                return keys;
            } else {
                return std::vector<std::string_view>{};
            }
        }
    }
    template<can_get_field T, typename Callable>
    void for_each_field(const T& obj, Callable&& func) {
        if constexpr (glz::reflectable<T>) {
            glz::for_each_field(obj, std::forward<Callable>(func));
        } else {
            if(obj.is_object()) {
                const glz::json_t::object_t& o = obj.get_object();
                for(auto& [_, value] : o) {
                    func(value);
                }
            }
        }
    }

    export template<can_get_field T>
    std::expected<std::string, std::string> get_field(const T& obj, std::string_view key) {
        std::string_view first_key = key;
        std::string_view second_key = "";
        if(auto pos = key.find('.'); pos != std::string::npos) {
            first_key = key.substr(0, pos);
            second_key = key.substr(pos + 1);
        }
        std::expected<std::string, std::string> result = std::unexpected("key not found: "+std::string(first_key));

        auto keys = get_keys(obj);
        unsigned int index = 0;
        for_each_field(obj, [&](auto&& field) {
            using decayed = std::decay_t<decltype(field)>;
            if(keys[index] == first_key) {
                if constexpr (can_get_field<decayed>) {
                    if(second_key.empty()) {
                        result = format_if_possible(field);
                    } else {
                        result = get_field(field, second_key);
                    }
                } else {
                    result = format_if_possible(field);
                }
            }
            index++;
        });
        return result.transform_error([&](auto&& err) {
            return std::string{first_key}+": "+err;
        });
    }

    template<typename T>
    concept can_stencil = can_get_field<T>;

    export template<can_stencil T>
    std::expected<std::string, std::string> stencil(std::string_view stencil, const T& obj) {
        std::string result{};
        std::string_view::size_type pos = 0;
        while(pos < stencil.size()) {
            char c = stencil[pos++];
            if(c == '{') {
                if(pos >= stencil.size()) {
                    return std::unexpected{"unexpected end of stencil"};
                }
                auto end = stencil.find('}', pos);
                if(end == std::string_view::npos) {
                    return std::unexpected{"missing '}'"};
                }
                std::string_view key = stencil.substr(pos, end - pos);
                auto field = get_field(obj, key);
                if(!field) {
                    return std::unexpected(field.error());
                }
                result.append(field.value());
                pos = end + 1;
            } else if(c == '\\') {
                if(pos >= stencil.size()) {
                    return std::unexpected{"unexpected end of stencil"};
                }
                c = stencil[pos++];
                if(c == '{' || c == '}' || c == '\\') {
                    result.push_back(c);
                } else if(c == 'n') {
                    result.push_back('\n');
                } else if(c == 't') {
                    result.push_back('\t');
                } else {
                    return std::unexpected{"invalid escape sequence"};
                }
            } else {
                result.push_back(c);
            }
        }
        return result;
    }

    static_assert(can_get_field<glz::json_t>);
}
