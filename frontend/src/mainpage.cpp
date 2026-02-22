import std;
import webpp;
import webxx;
import glaze;
import i18n;

import common;
import frontend.assets;
import frontend.components;
import frontend.pages;
import frontend.translations;
import frontend.utils;

using namespace mfk::i18n::literals;

namespace frontend {
    static mo_file translations_file = [](){
        std::string lang = *webpp::eval(R"(
            let lang = localStorage.getItem('language');
            if(lang === null || lang === undefined || lang === 'auto') {
                lang = window.navigator.language;
                if(lang === null || lang === undefined) {
                    lang = 'en';
                }
            }
            lang = lang.split('-')[0];
            lang
        )").get_property<std::string>("result");

        auto mo = load_translation(lang);
        if(mo) {
            webpp::log("Loaded translation for language {}.", lang);
            return *mo;
        } else {
            webpp::log("Failed to load translation for language {}: error code {}", lang, static_cast<int>(mo.error()));
        }
        return mo_file{};
    }();
}

namespace intl {
    const char* dgettext(const char* domainname, const char* msgid) {
        if(auto r = frontend::translations_file.lookup(msgid)) {
            return *r;
        }
        return nullptr;
    }
}

namespace frontend {

struct stats_data {
    unsigned int total_logs{};
    unsigned int total_attributes{};
    unsigned int total_resources{};
    unsigned int total_scopes{};
};

static r<common::log_entry> example_entry;
static r<common::logs_attributes_response> attributes;
static r<common::logs_scopes_response> scopes;
static r<common::logs_resources_response> resources;

static common::shared_settings settings;
static common::mmdb geoip_country;
static common::mmdb geoip_asn;
static common::mmdb geoip_city;
static std::vector<std::pair<std::string_view, common::mmdb*>> mmdbs = {
    {"country", &geoip_country},
    {"asn", &geoip_asn},
    {"city", &geoip_city},
};
static profile_data profile;

static pages::logs logs_page(profile, example_entry, attributes, scopes, resources, mmdbs);
static pages::table table_page(profile, example_entry, attributes, scopes, resources, mmdbs);
static pages::settings settings_page(profile, settings, example_entry, attributes, scopes, resources, mmdbs);

using page_tuple = std::tuple<std::string_view, std::string_view, std::string_view, pages::page*>;
static std::array all_pages = {
    page_tuple{"logs", "Text View"_, assets::icons::text_view, &logs_page},
    page_tuple{"table", "Table View"_, assets::icons::table_view, &table_page},
//    page_tuple{"analysis", "Analysis", assets::icons::analysis, &logs_page},
    page_tuple{"settings", "Settings"_, assets::icons::settings, &settings_page},
};

static pages::page* current_page = nullptr;

Webxx::dv page_stats(const stats_data& data);

auto refresh() -> webpp::coroutine<void> {
    auto refresh_button = *webpp::get_element_by_id("refresh-button");
    refresh_button.add_class("btn-disabled");
    refresh_button.add_class("*:animate-spin");
    webpp::get_element_by_id("stats")->add_class("animate-pulse");

    auto attributes_future = webpp::coro::fetch("/api/v1/logs/attributes", utils::fetch_options).then(std::mem_fn(&webpp::response::co_bytes));
    auto scopes_future = webpp::coro::fetch("/api/v1/logs/scopes", utils::fetch_options).then(std::mem_fn(&webpp::response::co_bytes));
    auto resources_future = webpp::coro::fetch("/api/v1/logs/resources", utils::fetch_options).then(std::mem_fn(&webpp::response::co_bytes));
    auto example_future = webpp::coro::fetch("/api/v1/logs?limit=1&attributes=*", utils::fetch_options).then(std::mem_fn(&webpp::response::co_bytes));

    attributes = glz::read<common::beve_opts, common::logs_attributes_response>(co_await attributes_future).value_or(common::logs_attributes_response{});
    scopes = glz::read<common::beve_opts, common::logs_scopes_response>(co_await scopes_future).value_or(common::logs_scopes_response{});
    resources = glz::read<common::beve_opts, common::logs_resources_response>(co_await resources_future).value_or(common::logs_resources_response{});

    auto resource_modals = Webxx::each(resources->resources, [](auto& e){
        return components::resource_modal{std::get<0>(e), std::get<0>(std::get<1>(e))};
    });
    webpp::get_element_by_id("modals-resources")->inner_html(Webxx::render(resource_modals));

    auto example = glz::read<common::beve_opts, common::logs_response>(co_await example_future).value_or(common::logs_response{});
    if(!example.logs.empty()) {
        example_entry = example.logs.front();
    } else {
        example_entry = common::log_entry{};
    }

    stats_data stats;
    stats.total_attributes = attributes->attributes.size();
    stats.total_logs = attributes->total_logs;
    stats.total_scopes = scopes->scopes.size();
    stats.total_resources = resources->resources.size();
    webpp::get_element_by_id("stats")->inner_html(Webxx::render(page_stats(stats)));

    // in theory we don't need this, because we rerender this area of the page ("stats") anyway
    refresh_button.remove_class("btn-disabled");
    refresh_button.remove_class("*:animate-spin");
    webpp::get_element_by_id("stats")->remove_class("animate-pulse");
    co_return;
}

auto theme_button() {
    using namespace Webxx;

    static event_context ctx;
    ctx.clear();
    return components::theme_button{ctx, profile};
}
void apply_theme(const std::string& theme) {
    webpp::eval("document.body.setAttribute('data-theme', '{}');", theme);
    webpp::get_element_by_id("theme_button")->inner_html(Webxx::render(theme_button()));
}

Webxx::dv page_stats(const stats_data& data) {
    static event_context ctx;
    ctx.clear();

    using namespace Webxx;
    return dv{{_class{"stats stats-vertical md:stats-horizontal shadow items-center"}},
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Log Entries"_},
            dv{{_class{"stat-value"}}, std::to_string(data.total_logs)}
        },
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Attributes"_},
            dv{{_class{"stat-value"}}, std::to_string(data.total_attributes)}
        },
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Resources"_},
            dv{{_class{"stat-value"}}, std::to_string(data.total_resources)}
        },
        dv{{_class{"stat"}},
            dv{{_class{"stat-title"}}, "Total Scopes"_},
            dv{{_class{"stat-value"}}, std::to_string(data.total_scopes)}
        },
        ctx.on_click(button{{_id{"refresh-button"}, _class{"btn btn-square size-20 p-2 m-2"}}, assets::icons::refresh},
            [](webpp::event) {
                webpp::coro::submit(refresh());
            }
        )
    };
}

void switch_page(pages::page* page, std::string_view id) {
    if(current_page == page) {
        return;
    }

    if(current_page) {
        current_page->close();
    }
    current_page = page;

    for(auto [page_id, name, icon, page_ptr] : all_pages) {
        std::string menu_id = "menu_" + std::string{page_id};
        webpp::get_element_by_id(menu_id)->remove_class("menu-active");
    }
    std::string menu_id = "menu_" + std::string{id};
    webpp::get_element_by_id(menu_id)->add_class("menu-active");

    webpp::get_element_by_id("page")->inner_html(Webxx::render(page->render()));
    webpp::set_timeout(std::chrono::milliseconds(0), [page](){page->open();});

    webpp::eval("document.location.hash = '#{}';", id);
    profile.set_data("current_page", std::string{id});
}

template<glz::string_literal identifier>
Webxx::dv profile_selector() {
    using namespace Webxx;

    static event_context ctx;
    ctx.clear();

    std::set<std::string> profiles_list;
    for(auto& p : profile.get_profiles()) {
        profiles_list.insert(p);
    }
    bool is_default = profile.get_current_profile() == "default";
    auto button_classes = is_default ? "btn btn-square btn-disabled" : "btn btn-square";

    static webpp::callback_data import_profile_callback([](auto, std::string_view data){
        using import_data = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
        auto imported_data = glz::read_json<import_data>(data);
        if(!imported_data) {
            webpp::eval("window.alert('{}: '+{});",
                std::string_view{"Invalid profile file"_},
                glz::write_json(glz::format_error(imported_data.error(), data)).value_or("error"));
            return;
        }
        if(imported_data->empty()) {
            webpp::eval("window.alert('{}');", std::string_view{"Empty profile file"_});
            return;
        }
        for(auto& [key, value] : *imported_data) {
            profile.set_profile_data(key, value);
        }
        profile.switch_profile(imported_data->begin()->first);
        webpp::get_element_by_id("profile_selector_mobile_container")->inner_html(Webxx::render(profile_selector<"mobile">()));
        webpp::get_element_by_id("profile_selector_desktop_container")->inner_html(Webxx::render(profile_selector<"desktop">()));
    }, false);

    return dv{{_class{"flex flex-row items-center gap-1"}},
        ctx.on_change(select{{_id{std::format("profile_selector_{}", identifier.sv())}, _class{"select select-lg grow"}},
            each(profiles_list, [](auto& p) {
                if(p == profile.get_current_profile()) {
                    return option{{_selected{}}, sanitize(p)};
                }
                return option{sanitize(p)};
            }),
            option{{_class{"italic"}}, "Create New Profile..."_},
            option{{_class{"italic"}}, "Import Profile..."_},
        }, [profiles_list](webpp::event e) {
            int selected = e.target().get_property<int>("selectedIndex").value_or(0);
            if(selected < profiles_list.size()) {
                std::string selected_profile = *std::next(profiles_list.begin(), selected);
                profile.switch_profile(selected_profile);

                webpp::get_element_by_id("profile_selector_mobile_container")->inner_html(Webxx::render(profile_selector<"mobile">()));
                webpp::get_element_by_id("profile_selector_desktop_container")->inner_html(Webxx::render(profile_selector<"desktop">()));
            } else if(selected == profiles_list.size()) {
                webpp::get_element_by_id("profile_name")->set_property("value", "");
                webpp::get_element_by_id("profile_add")->add_class("btn-disabled");
                webpp::eval("document.getElementById('dialog_add_profile').showModal();");
                e.prevent_default();
                e.target().set_property("selectedIndex", static_cast<int>(std::distance(profiles_list.begin(),
                    std::find(profiles_list.begin(), profiles_list.end(), profile.get_current_profile()))));
            } else {
                webpp::eval(R"(
                    let input = document.createElement('input');
                    input.type = 'file';
                    input.accept = 'application/json';
                    input.style.display = 'none';

                    document.body.appendChild(input);
                    input.addEventListener('change', (event)=>{{
                        let file = event.target.files[0];
                        if (!file) {{
                            return;
                        }}
                        let reader = new FileReader();
                        reader.onload = function(event) {{
                            let data = event.target.result;
                            window.invokeDataCallback(new Uint8Array(data), {});
                        }};
                        reader.readAsArrayBuffer(file);
                    }});
                    input.click();
                )", reinterpret_cast<std::uintptr_t>(&import_profile_callback));

                e.prevent_default();
                e.target().set_property("selectedIndex", static_cast<int>(std::distance(profiles_list.begin(),
                    std::find(profiles_list.begin(), profiles_list.end(), profile.get_current_profile()))));
            }
        }),
        dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Rename profile"_}},
            ctx.on_click(button{{_class{button_classes}}, assets::icons::edit}, [](webpp::event e) {
                webpp::get_element_by_id("dialog_rename_profile_title")->inner_html("Rename profile \"{}\""_(sanitize(profile.get_current_profile())));
                webpp::get_element_by_id("new_profile_name")->set_property("value", profile.get_current_profile());
                webpp::get_element_by_id("profile_rename")->add_class("btn-disabled");
                webpp::eval("document.getElementById('dialog_rename_profile').showModal();");
            }),
        },
        dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Export profile"_}},
            ctx.on_click(button{{_class{"btn btn-square"}}, assets::icons::export_}, [](webpp::event e) {
                std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data{};
                data[profile.get_current_profile()] = profile.get_profile_data();
                std::string json = glz::write<glz::opts{.format=glz::JSON, .prettify=true}>(data).value_or("error");

                webpp::eval(R"(
                    let a = document.createElement('a');
                    a.href = window.URL.createObjectURL(new Blob([{}], {{type: 'application/json'}}));
                    a.download = 'cutie-logs-profile-{}.json';

                    document.body.appendChild(a);
                    a.click();
                    document.body.removeChild(a);
                )", glz::write_json(json).value_or("\"error\""), profile.get_current_profile());
            }),
        },
        dv{{_class{"tooltip tooltip-bottom"}, _dataTip{"Delete profile"_}},
            ctx.on_click(button{{_class{button_classes}}, assets::icons::delete_}, [](webpp::event e) {
                webpp::get_element_by_id("dialog_delete_profile_title")->inner_html("Delete profile \"{}\""_(sanitize(profile.get_current_profile())));
                webpp::get_element_by_id("delete_profile_name")->set_property("placeholder", profile.get_current_profile());
                webpp::get_element_by_id("delete_profile_name")->set_property("value", "");
                webpp::get_element_by_id("profile_delete")->add_class("btn-disabled");
                webpp::eval("document.getElementById('dialog_delete_profile').showModal();");
            }),
        },
    };
}

Webxx::dialog dialog_add_profile(event_context& ctx) {
    using namespace Webxx;

    return dialog{{_id{"dialog_add_profile"}, _class{"modal"}},
        dv{{_class{"modal-box"}},
            form{{_method{"dialog"}},
                button{{_class{"btn btn-sm btn-circle btn-ghost absolute right-2 top-2"}}, "x"}
            },
            h3{{_class{"text-lg font-bold"}}, "Add profile"_},
            fieldset{{_class{"fieldset w-full"}},
                label{{_class{"fieldset-label"}}, "Profile name"_},
                ctx.on_input(input{{_id{"profile_name"}, _class{"input w-full validator"}, _required{}, _placeholder{"Name"_}}},
                    [](webpp::event e){
                        auto name = *e.target().as<webpp::element>()->get_property<std::string>("value");
                        bool valid = !name.empty() && !std::ranges::contains(profile.get_profiles(), name);
                        if(valid) {
                            webpp::get_element_by_id("profile_add")->remove_class("btn-disabled");
                            webpp::eval("document.getElementById('profile_name').setCustomValidity('');");
                        } else {
                            webpp::get_element_by_id("profile_add")->add_class("btn-disabled");
                            webpp::eval("document.getElementById('profile_name').setCustomValidity('{}');", std::string_view{"Profile name must be non-empty and unique."_});
                        }
                    }
                ),
                dv{{_id{"profile_name_validator"}, _class{"validator-hint"}}, "Profile name must be non-empty and unique."_},

                ctx.on_click(button{{_id{"profile_add"}, _class{"btn btn-success mt-4 w-fit btn-disabled"}},
                    assets::icons::add, "Add profile"_},
                    [](webpp::event e) {
                        webpp::eval("document.getElementById('dialog_add_profile').close();");

                        auto name = *webpp::get_element_by_id("profile_name")->get_property<std::string>("value");
                        if(name.empty()) {
                            return;
                        }
                        profile.switch_profile(name); // this will create the profile since it doesn't exist yet

                        std::string default_theme = *webpp::eval("let t = localStorage.getItem('default_theme'); if(t === null) {t = 'light';}; t").get_property<std::string>("result");
                        profile.set_data("theme", default_theme);
                        apply_theme(default_theme);

                        webpp::get_element_by_id("profile_selector_mobile_container")->inner_html(Webxx::render(profile_selector<"mobile">()));
                        webpp::get_element_by_id("profile_selector_desktop_container")->inner_html(Webxx::render(profile_selector<"desktop">()));
                    }
                ),
            }
        },
        form{{_method{"dialog"}, _class{"modal-backdrop"}},
            button{"close"}
        },
    };
}
Webxx::dialog dialog_rename_profile(event_context& ctx) {
    using namespace Webxx;

    return dialog{{_id{"dialog_rename_profile"}, _class{"modal"}},
        dv{{_class{"modal-box"}},
            form{{_method{"dialog"}},
                button{{_class{"btn btn-sm btn-circle btn-ghost absolute right-2 top-2"}}, "x"}
            },
            h3{{_id{"dialog_rename_profile_title"}, _class{"text-lg font-bold"}}, "Rename profile"_},
            fieldset{{_class{"fieldset w-full"}},
                label{{_class{"fieldset-label"}}, "New name"_},
                ctx.on_input(input{{_id{"new_profile_name"}, _class{"input w-full validator"}, _required{}, _placeholder{"Name"}}},
                    [](webpp::event e){
                        auto name = *e.target().as<webpp::element>()->get_property<std::string>("value");
                        bool valid = !name.empty() && !std::ranges::contains(profile.get_profiles(), name);
                        if(valid) {
                            webpp::get_element_by_id("profile_rename")->remove_class("btn-disabled");
                            webpp::eval("document.getElementById('new_profile_name').setCustomValidity('');");
                        } else {
                            webpp::get_element_by_id("profile_rename")->add_class("btn-disabled");
                            webpp::eval("document.getElementById('new_profile_name').setCustomValidity('{}');", std::string_view{"Profile name must be non-empty and unique."_});
                        }
                    }
                ),
                dv{{_id{"new_profile_name_validator"}, _class{"validator-hint"}}, "Profile name must be non-empty and unique."_},

                ctx.on_click(button{{_id{"profile_rename"}, _class{"btn btn-warning mt-4 w-fit btn-disabled"}},
                    assets::icons::edit, "Rename profile"_},
                    [](webpp::event e) {
                        webpp::eval("document.getElementById('dialog_rename_profile').close();");

                        auto name = *webpp::get_element_by_id("new_profile_name")->get_property<std::string>("value");
                        if(name.empty()) {
                            return;
                        }
                        profile.rename_profile(profile.get_current_profile(), name);

                        webpp::get_element_by_id("profile_selector_mobile_container")->inner_html(Webxx::render(profile_selector<"mobile">()));
                        webpp::get_element_by_id("profile_selector_desktop_container")->inner_html(Webxx::render(profile_selector<"desktop">()));
                    }
                ),
            }
        },
        form{{_method{"dialog"}, _class{"modal-backdrop"}},
            button{"close"}
        },
    };
}
Webxx::dialog dialog_delete_profile(event_context& ctx) {
    using namespace Webxx;

    return dialog{{_id{"dialog_delete_profile"}, _class{"modal"}},
        dv{{_class{"modal-box"}},
            form{{_method{"dialog"}},
                button{{_class{"btn btn-sm btn-circle btn-ghost absolute right-2 top-2"}}, "x"}
            },
            h3{{_id{"dialog_delete_profile_title"}, _class{"text-lg font-bold"}}, "Delete profile"_},
            fieldset{{_class{"fieldset w-full"}},
                label{{_class{"text-base"}}, "Please type the name of the profile to confirm deletion."_},
                ctx.on_input(input{{_id{"delete_profile_name"}, _class{"input w-full validator"}, _required{}, _placeholder{"Name"}}},
                    [](webpp::event e){
                        auto name = *e.target().as<webpp::element>()->get_property<std::string>("value");
                        bool valid = !name.empty() && name == profile.get_current_profile();
                        if(valid) {
                            webpp::get_element_by_id("profile_delete")->remove_class("btn-disabled");
                            webpp::eval("document.getElementById('delete_profile_name').setCustomValidity('');");
                        } else {
                            webpp::get_element_by_id("profile_delete")->add_class("btn-disabled");
                            webpp::eval("document.getElementById('delete_profile_name').setCustomValidity('{}');", std::string_view{"Profile name must match."_});
                        }
                    }
                ),
                dv{{_id{"profile_delete_name_validator"}, _class{"validator-hint"}}, "Profile name must match."_},

                ctx.on_click(button{{_id{"profile_delete"}, _class{"btn btn-error mt-4 w-fit btn-disabled"}},
                    assets::icons::delete_, "Delete profile"_},
                    [](webpp::event e) {
                        webpp::eval("document.getElementById('dialog_delete_profile').close();");

                        profile.delete_profile(profile.get_current_profile());
                        profile.switch_profile("default");

                        webpp::get_element_by_id("profile_selector_mobile_container")->inner_html(Webxx::render(profile_selector<"mobile">()));
                        webpp::get_element_by_id("profile_selector_desktop_container")->inner_html(Webxx::render(profile_selector<"desktop">()));
                    }
                ),
            }
        },
        form{{_method{"dialog"}, _class{"modal-backdrop"}},
            button{"close"}
        },
    };
}

auto page() {
    using namespace Webxx;

    static event_context ctx;
    ctx.clear();
    return fragment {
        dv{{_class{"flex flex-row items-baseline pt-4 pb-4 md:sticky top-0 z-10 bg-base-200"}},
            dv{{_class{"flex flex-col m-0 px-4 w-full md:w-auto gap-2"}},
                dv{{_class{"flex flex-row items-baseline m-0 p-0"}},
                    dv{{_class{"text-2xl font-bold"}}, common::project_name},
                    dv{{_class{"text-1xl font-bold ml-2"}}, "version {}"_(common::project_version)},
                },
                ul{{_class{"menu bg-base-300 md:menu-horizontal rounded-box w-full md:w-auto"}},
                    each(all_pages, [](auto& page) {
                        auto [id, name, icon, page_ptr] = page;
                        std::string menu_id = "menu_" + std::string{id};
                        return li{ctx.on_click(a{{_id{menu_id}}, icon, name}, [page_ptr, id](auto){switch_page(page_ptr, id);})};
                    }),
                },
                dv{{_id{"profile_selector_mobile_container"}, _class{"md:hidden *:w-full"}}, profile_selector<"mobile">()},
            },
            dv{{_id{"stats"}, _class{"ml-2 mr-2 grow flex flex-row justify-center items-center"}}, page_stats({})},
            dv{{_class{"flex items-center mr-4 gap-8"}},
                dv{{_id{"profile_selector_desktop_container"}, _class{"hidden md:flex"}}, profile_selector<"desktop">()},
                dv{{_id{"theme_button"}}, theme_button()},
            }
        },
        dv{{_class{"w-full mx-auto my-2 px-4 md:w-auto md:mx-[10vw]"}, {_id{"page"}}}},
        dv{{_id{"modals"}},
            dv{{_id{"modals-resources"}}},
            dialog_add_profile(ctx),
            dialog_rename_profile(ctx),
            dialog_delete_profile(ctx),
        },
    };
}

bool try_switch_page(std::string_view id) {
    for(auto [page_id, name, icon, page_ptr] : all_pages) {
        if(page_id == id) {
            switch_page(page_ptr, id);
            return true;
        }
    }
    return false;
}

void auto_select_page() {
    std::string page = webpp::eval("let page = document.location.hash; if(page === null) {page = '';}; page")["result"]
        .as<std::string>().value_or("");
    if(!page.empty()) {
        page = page.substr(1);
    }
    if(page == "none") {
        return;
    }
    if(try_switch_page(page)) {
        return;
    }

    page = profile.get_data("current_page").value_or("");
    if(page == "none") {
        return;
    }
    if(try_switch_page(page)) {
        return;
    }

    if(!all_pages.empty()) {
        auto [page_id, name, icon, page_ptr] = all_pages.front();
        switch_page(page_ptr, page_id);
    }
}

auto load_mmdb(common::mmdb& target, std::string_view url) -> webpp::coroutine<void> {
    webpp::log("Loading GeoIP database from {}...", url);
    auto response = co_await webpp::coro::fetch(url);
    webpp::log("Status for GeoIP database {}: {}, ok? {}", url, response.status(), response.ok());
    if(!response.ok()) {
        webpp::log("Failed to download GeoIP database from {}: HTTP {}", url, response.status());
        co_return;
    }

    auto data = co_await response.co_bytes();
    webpp::log("Finished downloading GeoIP database from {} ({} bytes), now parsing...", url, data.size());

    target = common::mmdb{std::move(data)};
    if(target.is_valid()) {
        common::mmdb::data d = target.get_metadata();
        auto json = glz::write_json(d.to_json()).value_or("error");
        webpp::log("Loaded GeoIP database: {}", json);
    } else {
        webpp::log("Failed to load GeoIP database: {}", target.get_error());
    }
}
auto load_settings() -> webpp::coroutine<void> {
    auto data = co_await webpp::coro::fetch("/api/v1/settings", utils::fetch_options)
        .then(std::mem_fn(&webpp::response::co_bytes));
    settings = glz::read<common::beve_opts, common::shared_settings>(data).value_or(common::shared_settings{});

    if(settings.geoip.country_url) {
        webpp::coro::submit(load_mmdb(geoip_country, *settings.geoip.country_url));
    }
    if(settings.geoip.asn_url) {
        webpp::coro::submit(load_mmdb(geoip_asn, *settings.geoip.asn_url));
    }
    if(settings.geoip.city_url) {
        webpp::coro::submit(load_mmdb(geoip_city, *settings.geoip.city_url));
    }
}

[[clang::export_name("main")]]
int main() {
    std::string theme = profile.get_data("theme").value_or("light");
    if(theme.empty()) {
        theme = "light";
        profile.set_data("theme", theme);
    }

    auto main = *webpp::get_element_by_id("main");
    main.set_property("data-theme", theme);
    main.inner_html(Webxx::render(page()));
    profile.add_callback([](const auto& profile) {
        std::string theme = profile.get_data("theme").value_or("light");
        apply_theme(theme);
    });

    auto_select_page();

    webpp::coro::submit(load_settings());
    webpp::coro::submit(refresh());

    return 0;
}

}
