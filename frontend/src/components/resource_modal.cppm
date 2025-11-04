export module frontend.components:resource_modal;

import std;
import webxx;
import glaze;
import i18n;

import common;

import :utils;

namespace frontend::components {

using namespace Webxx;
using namespace mfk::i18n::literals;

std::string resource_name(unsigned int id, const common::log_resource& resource) {
    // TODO: make this configurable
    if(resource.attributes.contains("service.name")) {
        return resource.attributes.at("service.name").get_string();
    }
    if(resource.attributes.contains("k8s.namespace.name")) {
        if(resource.attributes.contains("k8s.pod.name")) {
            return std::format("{}/{}", resource.attributes.at("k8s.namespace.name").get_string(), resource.attributes.at("k8s.pod.name").get_string());
        }
    }
    return "Resource #{}"_(id);
}

export struct resource_modal : component<resource_modal> {
    static Webxx::fragment render_attribute_value(const glz::generic& v) {
        using namespace Webxx;
        if(v.is_boolean()) {
            return fragment{
                span{{_class{v.get_boolean()?"text-success":"text-error"}}, v.get_boolean()?"true":"false"}
            };
        }
        if(v.is_array()) {
            unsigned int index = 0;
            return fragment{
                table{{_class{"table table-zebra w-full border-l-3 border-base-300"}},
                    each(v.get_array(), [&](const auto& e){
                        return tr{
                            td{{_class{"text-right w-0"}}, std::format("{}", index++)},
                            td{render_attribute_value(e)}
                        };
                    })
                }
            };
        }
        if(v.is_object()) {
            std::set<std::string> keys;
            for(auto& [key, value] : v.get_object()) {
                keys.insert(key);
            }
            return fragment{
                table{{_class{"table table-zebra w-full border-l-3 border-base-300"}},
                    each(keys, [&](const auto& key){
                        return tr{
                            td{{_class{"font-bold w-0"}}, key},
                            td{render_attribute_value(v[key])}
                        };
                    })
                }
            };
        }
        if(v.is_null()) {
            return fragment{
                i{"null"}
            };
        }
        // this will handle strings (quoted :D) and numbers
        return fragment{v.dump().value_or("error")};
    }

    resource_modal(unsigned int id, const common::log_resource& resource) : component<resource_modal>{
        [&](){
            std::set<std::string> attributes;
            for(auto& [key, value] : resource.attributes.get_object()) {
                attributes.insert(key);
            }

            return dialog{{_id{std::format("modal_resource_{}", id)}, _class{"modal"}},
                dv{{_class{"modal-box w-11/12 max-w-5xl"}},
                    form{{_method{"dialog"}},
                        button{{_class{"btn btn-sm btn-circle btn-ghost absolute right-2 top-2"}}, "x"}
                    },
                    h3{{_class{"text-lg font-bold"}}, std::format("Resource \"{}\" ({})", resource_name(id, resource), id)},
                    dv{{_class{"overflow-auto"}},
                        table{{_class{"table table-zebra w-full"}},
                            thead{tr{
                                th{"Attribute"_},
                                th{"Value"_}
                            }},
                            Webxx::each(attributes, [&resource](auto& key){
                                auto attr = resource.attributes[key];
                                return tr{
                                    td{{_class{"font-bold w-0"}}, key},
                                    td{render_attribute_value(attr)}
                                };
                            })
                        }
                    }
                },
                form{{_method{"dialog"}, _class{"modal-backdrop"}},
                    button{"close"}
                },
            };
        }()
    } {}
};

}
