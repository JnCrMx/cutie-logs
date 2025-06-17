export module common:glaze;

import glaze;

namespace common {

export constexpr auto beve_opts = glz::opts{.format = glz::BEVE, .structs_as_arrays = true};
export constexpr auto json_opts = glz::opts{.format = glz::JSON, .minified = true};

}
