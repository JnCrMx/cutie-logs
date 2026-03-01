module;
#include <chrono>
#include <expected>
#include <format>
#include <string_view>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
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
        } else if constexpr (std::is_same_v<T, glz::generic>) {
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
    concept can_get_field = glz::reflectable<std::remove_pointer_t<std::decay_t<T>>> || std::same_as<T, glz::generic>;

    template<can_get_field T>
    auto get_keys(const T& obj) {
        if constexpr (glz::reflectable<std::remove_pointer_t<std::decay_t<T>>>) {
            return glz::reflect<std::remove_pointer_t<std::decay_t<T>>>::keys;
        } else {
            if(obj.is_object()) {
                const glz::generic::object_t& o = obj.get_object();
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
        if constexpr (glz::reflectable<std::decay_t<T>>) {
            glz::for_each_field(obj, std::forward<Callable>(func));
        } else if constexpr (glz::reflectable<std::remove_pointer_t<std::decay_t<T>>>) {
            if(obj == nullptr) {
                return; // TODO: return error?
            }
            glz::for_each_field(*obj, std::forward<Callable>(func));
        } else {
            if(obj.is_object()) {
                const glz::generic::object_t& o = obj.get_object();
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
        bool key_found = false;
        std::expected<std::string, std::string> result = std::unexpected("key not found: "+std::string(first_key));
        if(first_key.ends_with("?")) {
            first_key = first_key.substr(0, first_key.size() - 1);
            result = "";
        }
        if(first_key.empty()) {
            if(key.empty()) {
                return eval_expression(obj, expression, functions, functions);
            }

            if constexpr (has_root<T>) {
                first_key = T::root;
            } else  {
                return std::unexpected{std::format("\".\" given as key, but type \"{}\" does not have a root", glz::name_v<T>)};
            }
        }

        auto keys = get_keys(obj);
        unsigned int index = 0;
        for_each_field<T>(obj, [&](auto&& field) {
            using decayed = std::decay_t<decltype(field)>;
            if(keys[index] == first_key) {
                key_found = true;
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

        if constexpr (has_root<T>) {
            if(!key_found) {
                unsigned int index = 0;
                for_each_field<T>(obj, [&](auto&& field) {
                    using decayed = std::decay_t<decltype(field)>;
                    if(keys[index] == T::root) {
                        result = get_field(field, key, functions, expression);
                    }
                    index++;
                });
            }
        }

        return result.transform_error([&](auto&& err) {
            return std::string{first_key}+": "+err;
        });
    }

    template<typename T>
    concept can_stencil = can_get_field<T>;

    enum class if_end_type {
        else_,
        end_
    };
    std::optional<std::pair<std::string_view::size_type, if_end_type>> find_if_end(std::string_view stencil, std::string_view::size_type pos) {
        int depth = 0;
        while(pos < stencil.size()) {
            char c = stencil[pos++];
            if(c == '{') {
                if(pos >= stencil.size()) {
                    return std::nullopt;
                }
                auto end = stencil.find('}', pos);
                if(end == std::string_view::npos) {
                    return std::nullopt;
                }
                std::string_view key = stencil.substr(pos, end - pos);
                if(key.starts_with("?")) {
                    depth++;
                } else if(key == ":?") {
                    if(depth == 0) {
                        return std::make_pair(end+1, if_end_type::else_);
                    }
                } else if(key == "/?") {
                    if(depth == 0) {
                        return std::make_pair(end+1, if_end_type::end_);
                    }
                    depth--;
                }
                pos = end + 1;
            }
        }
        return std::nullopt;
    }

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

                if(key.starts_with("?")) {
                    key.remove_prefix(1);
                    auto field = get_field(obj, key, functions, expression);
                    if(!field) {
                        return std::unexpected(field.error());
                    }

                    if(*field == "true") {
                        pos = end + 1;
                    } else if(*field == "false") {
                        auto if_end = find_if_end(stencil, end+1);
                        if(!if_end) {
                            return std::unexpected("Could not find else or end tag");
                        }
                        pos = if_end->first;
                        continue;
                    } else {
                        return std::unexpected(std::format("Boolean expression \"{}\" is not either \"true\" or \"false\".", *field));
                    }
                } else if(key == ":?") { // if this is encountered, the boolean expression evaluated to true and we should skip to the end
                    auto if_end = find_if_end(stencil, end+1);
                    if(!if_end || if_end->second != if_end_type::end_) {
                        return std::unexpected("Could not find end tag");
                    }
                    pos = if_end->first;
                    continue;
                } else if(key == "/?") {
                    pos = end + 1;
                } else {
                    auto field = get_field(obj, key, functions, expression);
                    if(!field) {
                        return std::unexpected(field.error());
                    }

                    result.append(field.value());
                    pos = end + 1;
                }
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

    export std::expected<std::unordered_set<std::string_view>, std::string> stencil_required_attributes(std::string_view stencil) {
        std::unordered_set<std::string_view> result{};
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
                if(auto pos = key.find('|'); pos != std::string_view::npos) {
                    key = trim(key.substr(0, pos));
                }

                if(key.starts_with("?")) {
                    key.remove_prefix(1);
                    if(key.ends_with("?")) {
                        key.remove_suffix(1);
                    }
                    result.emplace(key);
                } else if(key == ":?" || key == "/?") {
                    // ignore these keys
                } else {
                    if(key.ends_with("?")) {
                        key.remove_suffix(1);
                    }
                    result.emplace(key);
                }
                pos = end + 1;
            }
        }
        return result;
    }

    export template<can_stencil T, glz::reflectable Functions = stencil_functions>
    glz::generic stencil_json(glz::generic stencil_object, const T& obj, const Functions& functions = stencil_functions{}) {
        struct visitor {
            const T& obj;
            const Functions& functions;

            void operator()(glz::generic::null_t&) const {}
            void operator()(double&) const {}
            void operator()(bool&) const {}

            void operator()(std::string& str) const {
                auto r = stencil(str, obj, functions);
                if(!r) {
                    str = std::format("stencil error: {}", r.error());
                } else {
                    str = r.value();
                }
            }

            void operator()(glz::generic::array_t& array) const {
                for(auto& value : array) {
                    (*this)(value);
                }
            }
            void operator()(glz::generic::object_t& obj) const {
                for(auto& [key, value] : obj) {
                    (*this)(value);
                }
            }

            void operator()(glz::generic& json) const {
                if(json.is_string() && json.get_string().ends_with("!json")) {
                    std::string str = json.get_string();
                    str = str.substr(0, str.size() - 5); // remove the "!json" suffix

                    auto r = stencil(str, this->obj, this->functions);
                    if(!r) {
                        json = glz::generic::null_t{};
                    } else if(auto p = glz::read_json<glz::generic>(r.value())) {
                        json = std::move(p.value());
                    } else {
                        json = glz::generic::null_t{};
                    }
                } else {
                    std::visit(*this, *json);
                }
            }
        };
        visitor v{obj, functions};
        v(stencil_object);

        return stencil_object;
    }

    static_assert(can_get_field<glz::generic>);
}
