module;
#include <glaze/glaze.hpp>

export module glaze;

export namespace glz {
    using glz::write_json;
    using glz::read_json;
    using glz::json_t;

    using glz::meta;
    using glz::opts;
}
