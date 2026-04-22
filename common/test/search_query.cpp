#include <cassert>
#include <iostream>
#include <memory>

import common;
import glaze;

template<auto options> void roundtrip(const common::search_query& q) {
    std::string serialized = glz::write<options>(q).value();

    common::search_query deserialized{};
    assert(!glz::read<options>(deserialized, serialized));

    std::string serialized2 = glz::write<options>(deserialized).value();
    assert(serialized == serialized2);
};

int main() {
    common::search_query q = common::and_query{
        {
            common::body_query{"test"},
            common::not_query{
                common::or_query{
                    {
                        common::body_query{"test2"},
                        common::attribute_query{"ClientHost", {common::search_type::text_contains}, "127.0.0.1"}
                    }
                }
            }
        }
    };
    std::cout << glz::write_json(q).value() << std::endl;

    roundtrip<common::json_opts>(q);
    roundtrip<common::beve_opts>(q);

    return 0;
}
