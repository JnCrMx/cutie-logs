module;
#include <chrono>
#include <expected>
#include <format>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

export module common:stencil;

import glaze;
import :structs;
import :stencil_functions;

namespace common {
    template<class Lambda, int=(Lambda{}(), 0)>
    constexpr bool is_constexpr(Lambda) { return true; }
    constexpr bool is_constexpr(...) { return false; }

    template<typename T, glz::string_literal fmt = "{}">
    struct test_format {
        static constexpr bool value = is_constexpr([](){
            std::format_string<T> test{fmt};
            return true;
        });
    };

    // required with libc++-wasi
    template<typename Clock, typename Duration>
    struct test_format<std::chrono::time_point<Clock, Duration>, "{}"> {
        static constexpr bool value = false;
    };

    export template<typename T>
    concept trivially_formattable = std::formattable<T, char> && test_format<T>::value;

    template<typename T>
    std::expected<std::string, std::string> format_if_possible(const T& obj) {
        if constexpr (trivially_formattable<T>) {
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

    std::string_view trim(std::string_view str) {
        while(!str.empty() && str.front() == ' ') {
            str.remove_prefix(1);
        }
        while(!str.empty() && str.back() == ' ') {
            str.remove_suffix(1);
        }
        return str;
    }

    export template<typename T>
    concept has_base = requires(T t) {
        { t.base } -> glz::reflectable;
    };
    export template<typename T>
    concept has_base_function = requires(T t) {
        { t.base() } -> glz::reflectable;
    };

    export template<typename T>
    std::expected<std::string, std::string> eval_expression(T&& value, std::string_view expression, glz::reflectable auto functions, glz::reflectable auto functions_root) {
        if(expression.empty()) {
            return format_if_possible(value);
        }
        std::string_view first_expression = trim(expression);
        std::string_view second_expression = "";
        if(auto pos = expression.find('|'); pos != std::string::npos) {
            first_expression = expression.substr(0, pos);
            second_expression = expression.substr(pos + 1);
        }
        first_expression = trim(first_expression);

        std::optional<std::string_view> arg{};
        if(auto pos = first_expression.find('('); pos != std::string::npos) {
            if(!first_expression.ends_with(')')) {
                return std::unexpected{"missing ')'"};
            }
            arg = first_expression.substr(pos + 1, first_expression.size() - pos - 2);
            first_expression = first_expression.substr(0, pos);
        }

        auto keys = get_keys(functions);
        unsigned int index = 0;
        std::expected<std::string, std::string> result = std::unexpected(std::format("cannot find function \"{}\"", first_expression));
        bool found_one = false;
        for_each_field(functions, [&](auto&& field) {
            using decayed = std::decay_t<decltype(field)>;
            if(keys[index] == first_expression) {
                found_one = true;
                if constexpr (std::is_invocable_v<decltype(field), T&&>) {
                    if(arg) {
                        result = std::unexpected(std::format("function \"{}\" does not take arguments", first_expression));
                        return;
                    }
                    auto x = field(std::forward<T>(value));
                    // TODO: implement error handling (field could return std::expected and then we could optionally unmarshal it, unless the next field takes a std::expected as well)
                    if(second_expression.empty()) {
                        result = format_if_possible(x);
                    } else {
                        result = eval_expression(x, second_expression, functions_root, functions_root);
                    }
                } else if constexpr (std::is_invocable_v<decltype(field), T&&, std::string_view>) {
                    if(!arg) {
                        result = std::unexpected(std::format("function \"{}\" requires an argument", first_expression));
                        return;
                    }
                    auto x = field(std::forward<T>(value), *arg);
                    if(second_expression.empty()) {
                        result = format_if_possible(x);
                    } else {
                        result = eval_expression(x, second_expression, functions_root, functions_root);
                    }
                } else {
                    result = std::unexpected(std::format("cannot call function \"{}\" with argument of type \"{}\"",
                        first_expression, glz::name_v<std::decay_t<T>>));
                }
            }
            index++;
        });
        if(!found_one) {
            if constexpr (has_base_function<decltype(functions)>) {
                return eval_expression(value, expression, functions.base(), functions_root);
            } else if constexpr (has_base<decltype(functions)>) {
                return eval_expression(value, expression, functions.base, functions_root);
            }
        }

        return result;
    }

    template<typename T>
    concept has_root = requires() {
        { T::root } -> std::convertible_to<std::string_view>;
    };

    export template<can_get_field T, glz::reflectable Functions = stencil_functions>
    std::expected<std::string, std::string> get_field(const T& obj, std::string_view key,
        const Functions& functions = stencil_functions{}, std::string_view expression = "")
    {
        std::string_view first_key = key;
        std::string_view second_key = "";
        if(auto pos = key.find('.'); pos != std::string::npos) {
            first_key = key.substr(0, pos);
            second_key = key.substr(pos + 1);
        }
        std::expected<std::string, std::string> result = std::unexpected("key not found: "+std::string(first_key));
        if(first_key.ends_with("?")) {
            first_key = first_key.substr(0, first_key.size() - 1);
            result = "";
        }
        if(first_key.empty()) {
            if constexpr (has_root<T>) {
                first_key = T::root;
            } else if (key.empty()) {
                return eval_expression(obj, expression, functions, functions);
            } else {
                return std::unexpected{std::format("\".\" given as key, but type \"{}\" does not have a root", glz::name_v<T>)};
            }
        }

        auto keys = get_keys(obj);
        unsigned int index = 0;
        for_each_field(obj, [&](auto&& field) {
            using decayed = std::decay_t<decltype(field)>;
            if(keys[index] == first_key) {
                if constexpr (can_get_field<decayed>) {
                    if(second_key.empty()) {
                        result = eval_expression(field, expression, functions, functions);
                    } else {
                        result = get_field(field, second_key, functions, expression);
                    }
                } else {
                    result = eval_expression(field, expression, functions, functions);
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

    export template<can_stencil T, glz::reflectable Functions = stencil_functions>
    std::expected<std::string, std::string> stencil(std::string_view stencil, const T& obj, const Functions& functions = stencil_functions{}) {
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
                std::string_view expression{};
                if(auto pos = key.find('|'); pos != std::string_view::npos) {
                    expression = trim(key.substr(pos + 1));
                    key = trim(key.substr(0, pos));
                }
                auto field = get_field(obj, key, functions, expression);
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
