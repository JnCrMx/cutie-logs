export module common:glaze;

import glaze;

namespace common {

struct my_beve_opts : glz::opts {
    bool structs_as_arrays;
};

export constexpr auto beve_opts = [](){
    my_beve_opts opts{};
    opts.format = glz::BEVE;
    opts.structs_as_arrays = true;
    return opts;
}();
export constexpr auto json_opts = glz::opts{.format = glz::JSON, .minified = true};

}
