export module frontend.components:resource_modal;

import std;
import webxx;
import glaze;
import i18n;

import common;

import :utils;
import :json_table;

namespace frontend::components {

using namespace Webxx;
using namespace mfk::i18n::literals;

export struct resource_modal : component<resource_modal> {
    resource_modal(unsigned int id, const common::log_resource& resource) : component<resource_modal>{
        [&](){
            std::set<std::string> attributes;
            for(auto& [key, value] : resource.attributes.get_object()) {
                attributes.insert(key);
            }

            return dialog{{_id{std::format("modal_resource_{}", id)}, _class{"modal"}},
                dv{{_class{"modal-box w-11/12 max-w-5xl max-h-[75vh]"}},
                    form{{_method{"dialog"}},
                        button{{_class{"btn btn-sm btn-circle btn-ghost absolute right-2 top-2"}}, "x"}
                    },
                    h3{{_class{"text-lg font-bold"}}, "Resource \"{}\" (#{})"_(sanitize(resource_name(resource)), id)},
                    dv{{_class{"overflow-auto"}},
                        table{{_class{"table table-zebra w-full"}},
                            thead{tr{
                                th{"Attribute"_},
                                th{"Value"_}
                            }},
                            Webxx::each(attributes, [&resource](auto& key){
                                auto attr = resource.attributes[key];
                                return tr{
                                    td{{_class{"font-bold w-0"}}, sanitize(key)},
                                    td{json_table(attr)}
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
