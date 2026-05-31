export module frontend.components:json_table;

import std;
import webxx;
import glaze;
import i18n;

import :utils;

namespace frontend::components {
    using namespace mfk::i18n::literals;

    export Webxx::fragment json_table(const glz::generic& v) {
        using namespace Webxx;
        if(v.is_boolean()) {
            return fragment{
                span{{_class{v.get_boolean() ? "text-success" : "text-error"}},
                    v.get_boolean() ? "true"_ : "false"_
                }
            };
        }
        if(v.is_array()) {
            unsigned int index = 0;
            return fragment{
                table{{_class{"table table-zebra w-full border-l-3 border-base-300"}},
                    each(v.get_array(), [&](const auto& e){
                        return tr{
                            td{{_class{"text-right w-0"}}, std::format("{}", index++)},
                            td{json_table(e)}
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
                            td{{_class{"font-bold w-0"}}, sanitize(key)},
                            td{json_table(v[key])}
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
        return fragment{sanitize(v.dump().value_or("error"))};
    }
}
