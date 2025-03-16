module;
#if BUILD_TARGET_frontend
#include "fake_std.hpp"
#endif
#include <glaze/glaze.hpp>

export module glaze;

export namespace glz {
    using glz::write_json;
    using glz::read_json;
    using glz::write_beve;
    using glz::read_beve;
    using glz::format_error;
    using glz::error_code;
    using glz::error_ctx;

    using glz::json_t;

    using glz::write;
    using glz::read;
    using glz::meta;
    using glz::opts;

    using glz::reflect;
    using glz::reflectable;

    using glz::stencil;
}
