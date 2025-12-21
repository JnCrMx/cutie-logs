export module frontend.components:utils;

import std;
import webpp;
import webxx;
import glaze;

namespace Webxx {
    constexpr static char customDataTipAttr[] = "data-tip";
    export using _dataTip = attr<customDataTipAttr>;

    constexpr static char customDataThemeAttr[] = "data-theme";
    export using _dataTheme = attr<customDataThemeAttr>;

    constexpr static char customDataCallbackAttr[] = "data-callback";
    export using _dataCallback = attr<customDataCallbackAttr>;

    constexpr static char customAriaLabelAttr[] = "aria-label";
    export using _ariaLabel = attr<customAriaLabelAttr>;

    constexpr static char onClickAttr[] = "onclick";
    export using _onClick = attr<onClickAttr>;

    constexpr static char onInputAttr[] = "oninput";
    export using _onInput = attr<onInputAttr>;

    constexpr static char onChangeAttr[] = "onchange";
    export using _onChange = attr<onChangeAttr>;

    constexpr static char onDragStartAttr[] = "ondragstart";
    export using _onDragStart = attr<onDragStartAttr>;

    constexpr static char onDragEndAttr[] = "ondragend";
    export using _onDragEnd = attr<onDragEndAttr>;

    constexpr static char onDragOverAttr[] = "ondragover";
    export using _onDragOver = attr<onDragOverAttr>;

    constexpr static char onDragEnterAttr[] = "ondragenter";
    export using _onDragEnter = attr<onDragEnterAttr>;

    constexpr static char onDragLeaveAttr[] = "ondragleave";
    export using _onDragLeave = attr<onDragLeaveAttr>;
}

namespace frontend {

export using click_event = Webxx::_onClick;
export using input_event = Webxx::_onInput;
export using change_event = Webxx::_onChange;
export using drag_start_event = Webxx::_onDragStart;
export using drag_end_event = Webxx::_onDragEnd;
export using drag_over_event = Webxx::_onDragOver;
export using drag_enter_event = Webxx::_onDragEnter;
export using drag_leave_event = Webxx::_onDragLeave;

export namespace events {
    template<typename EventAttribute>
    auto on_event(auto el, webpp::callback_data* cb) {
        el.data.attributes.push_back(EventAttribute{std::format("handleEvent(this, event, {});", reinterpret_cast<std::uintptr_t>(cb))});
        return el;
    }

    auto on_click(auto el, webpp::callback_data* cb) {
        return on_event<Webxx::_onClick>(el, cb);
    }
    auto on_input(auto el, webpp::callback_data* cb) {
        return on_event<Webxx::_onInput>(el, cb);
    }
    auto on_change(auto el, webpp::callback_data* cb) {
        return on_event<Webxx::_onChange>(el, cb);
    }
    auto on_drag_start(auto el, webpp::callback_data* cb) {
        return on_event<Webxx::_onDragStart>(el, cb);
    }
    auto on_drag_end(auto el, webpp::callback_data* cb) {
        return on_event<Webxx::_onDragEnd>(el, cb);
    }
    auto on_drag_over(auto el, webpp::callback_data* cb) {
        return on_event<Webxx::_onDragOver>(el, cb);
    }
    auto on_drag_enter(auto el, webpp::callback_data* cb) {
        return on_event<Webxx::_onDragEnter>(el, cb);
    }
    auto on_drag_leave(auto el, webpp::callback_data* cb) {
        return on_event<Webxx::_onDragLeave>(el, cb);
    }

    template<typename Event>
    auto on_events(auto el, std::function<void(webpp::event)> cb) {
        return on_event<Event>(el, std::move(cb));
    }

    template<typename Event, typename... Events, typename... Funcs>
    auto on_events(auto el, std::function<void(webpp::event)> cb, Funcs... more) {
        return on_events<Events...>(on_event<Event>(el, std::move(cb)), std::move(more)...);
    }
}

export class event_context {
    public:
        using event_callback = std::function<void(webpp::event)>;

        auto on_click(auto el, event_callback&& cb) {
            return on_event<Webxx::_onClick>(std::move(el), std::move(cb));
        }
        auto on_input(auto el, event_callback&& cb) {
            return on_event<Webxx::_onInput>(std::move(el), std::move(cb));
        }
        auto on_change(auto el, event_callback&& cb) {
            return on_event<Webxx::_onChange>(std::move(el), std::move(cb));
        }
        auto on_drag_start(auto el, event_callback&& cb) {
            return on_event<Webxx::_onDragStart>(std::move(el), std::move(cb));
        }
        auto on_drag_end(auto el, event_callback&& cb) {
            return on_event<Webxx::_onDragEnd>(std::move(el), std::move(cb));
        }
        auto on_drag_over(auto el, event_callback&& cb) {
            return on_event<Webxx::_onDragOver>(std::move(el), std::move(cb));
        }
        auto on_drag_enter(auto el, event_callback&& cb) {
            return on_event<Webxx::_onDragEnter>(std::move(el), std::move(cb));
        }
        auto on_drag_leave(auto el, event_callback&& cb) {
            return on_event<Webxx::_onDragLeave>(std::move(el), std::move(cb));
        }

        template<typename EventAttribute>
        auto on_event(auto el, event_callback&& cb) {
            using js_handle = std::decay_t<decltype(std::declval<webpp::event>().handle())>;
            auto& data = callbacks.emplace_back(std::make_unique<webpp::callback_data>([cb = std::move(cb)](js_handle handle, std::string_view){
                cb(webpp::event{handle});
            }, false));
            return events::on_event<EventAttribute>(el, data.get());
        }

        template<typename Event>
        auto on_events(auto el, std::function<void(webpp::event)> cb) {
            return on_event<Event>(el, std::move(cb));
        }

        template<typename Event, typename... Events, typename... Funcs>
        auto on_events(auto el, std::function<void(webpp::event)> cb, Funcs... more) {
            return on_events<Events...>(on_event<Event>(el, std::move(cb)), std::move(more)...);
        }

        void clear() {
            callbacks.clear();
        }
    private:
        std::vector<std::unique_ptr<webpp::callback_data>> callbacks;
};

export template<typename T>
struct event_data {
    T data;
    std::vector<std::function<void(event_data&)>> callbacks;

    void add_callback(std::function<void(event_data&)> cb) {
        callbacks.push_back(std::move(cb));
    }
    void update() {
        for (auto& cb : callbacks) {
            cb(*this);
        }
    }
    void set_value(const T& value) {
        data = value;
        update();
    }
    void set_value(T&& value) {
        data = std::move(value);
        update();
    }

    T& operator=(const T& value) {
        data = value;
        update();
        return data;
    }
    T& operator=(T&& value) {
        data = std::move(value);
        update();
        return data;
    }

    T& operator*() {
        return data;
    }
    const T& operator*() const {
        return data;
    }

    T* operator->() {
        return &data;
    }
    const T* operator->() const {
        return &data;
    }
};
export template<typename T> using r = event_data<T>;

export class profile_data {
    public:
        profile_data()
        {
            std::string profiles_json = *webpp::eval("let profiles = localStorage.getItem('profiles'); if(profiles === null) {{profiles = '{}';}}; profiles").get_property<std::string>("result");
            profiles = glz::read_json<decltype(profiles)>(profiles_json).value_or(decltype(profiles){});
            current_profile = *webpp::eval("let p = localStorage.getItem('current_profile'); if(p === null) {p = 'default';}; p").get_property<std::string>("result");
            if(!profiles.contains(current_profile)) {
                profiles[current_profile] = {};
            }
            webpp::log("Current profile: {}", current_profile);
        }

        void switch_profile(std::string name) {
            this->current_profile = std::move(name);
            if(!profiles.contains(current_profile)) {
                profiles[current_profile] = {};
            }
            for(auto& cb : callbacks) {
                cb(*this);
            }
            save();
        }
        void delete_profile(const std::string& name) {
            if(profiles.contains(name)) {
                profiles.erase(name);
            }
            if(current_profile == name) {
                current_profile = "default";
            }
            save();
        }
        void rename_profile(const std::string& old_name, const std::string& new_name) {
            if(old_name == new_name) {
                return;
            }

            if(profiles.contains(old_name)) {
                profiles[new_name] = std::move(profiles[old_name]);
                profiles.erase(old_name);
            }
            if(current_profile == old_name) {
                current_profile = new_name;
            }
            save();
        }

        const std::string& get_current_profile() const {
            return current_profile;
        }
        auto get_profiles() const {
            return profiles | std::views::keys;
        }
        void add_callback(std::function<void(profile_data&)> cb) {
            callbacks.push_back(std::move(cb));
        }
        const std::unordered_map<std::string, std::string>& get_profile_data() const {
            return profiles.at(current_profile);
        }
        const std::unordered_map<std::string, std::string>& get_profile_data(const std::string& name) const {
            return profiles.at(name);
        }
        void set_profile_data(const std::string& name, const std::unordered_map<std::string, std::string>& data) {
            profiles[name] = data;
            save();
        }

        std::optional<std::string> get_data(const std::string& key) const {
            const auto& data = profiles.at(current_profile);
            auto it = data.find(key);
            if (it != data.end()) {
                return it->second;
            }
            return std::nullopt;
        }
        void set_data(const std::string& key, const std::string& value) {
            profiles[current_profile][key] = value;
            save();
        }
        void remove_data(const std::string& key) {
            profiles[current_profile].erase(key);
            save();
        }
    private:
        void save() {
            std::string profiles_json = glz::write_json(profiles).value_or("{}");
            webpp::eval("window.localStorage.setItem('profiles', {});", glz::write_json(profiles_json).value_or("\"{}\"")); // for escaping
            webpp::eval("window.localStorage.setItem('current_profile', '{}');", current_profile);
        }

        using data = std::unordered_map<std::string, std::string>;

        std::string current_profile;
        std::unordered_map<std::string, data> profiles;
        std::vector<std::function<void(profile_data&)>> callbacks;
};

export std::string sanitize(std::string_view sv) {
    std::string out{};
    out.reserve(sv.size());
    for(auto c : sv) {
        switch(c) {
            case '<':
                out.append("&lt;");
                break;
            case '>':
                out.append("&gt;");
                break;
            case '&':
                out.append("&amp;");
                break;
            default:
                out.append({c});
        }
    }
    return out;
}

}
