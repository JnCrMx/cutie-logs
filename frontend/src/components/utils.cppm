export module frontend.components:utils;

import std;
import web;
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
}

namespace frontend {

export class event_context {
    public:
        auto on_click(auto el, web::event_callback&& cb) {
            return on_event<Webxx::_onClick>(std::move(el), std::move(cb));
        }
        auto on_input(auto el, web::event_callback&& cb) {
            return on_event<Webxx::_onInput>(std::move(el), std::move(cb));
        }
        auto on_change(auto el, web::event_callback&& cb) {
            return on_event<Webxx::_onChange>(std::move(el), std::move(cb));
        }

        template<typename EventAttribute>
        auto on_event(auto el, web::event_callback&& cb) {
            auto& data = callbacks.emplace_back(std::make_unique<web::callback_data>(std::move(cb), false));
            el.data.attributes.push_back(Webxx::_dataCallback{std::format("{}", reinterpret_cast<std::uintptr_t>(data.get()))});
            el.data.attributes.push_back(EventAttribute{"handleEvent(this, event);"});
            return el;
        }

        void clear() {
            callbacks.clear();
        }
    private:
        std::vector<std::unique_ptr<web::callback_data>> callbacks;
};

export template<typename T>
struct event_data {
    T data;
    std::vector<std::function<void(event_data&)>> callbacks;

    void add_callback(std::function<void(event_data&)> cb) {
        callbacks.push_back(std::move(cb));
    }
    T& operator=(const T& value) {
        data = value;
        for (auto& cb : callbacks) {
            cb(*this);
        }
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
