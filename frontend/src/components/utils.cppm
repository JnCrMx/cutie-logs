export module frontend.components:utils;

import std;
import webpp;
import webxx;

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
            el.data.attributes.push_back(EventAttribute{std::format("handleEvent(this, event, {});", reinterpret_cast<std::uintptr_t>(data.get()))});
            return el;
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

}
