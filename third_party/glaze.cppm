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
    using glz::read_supported;
    using glz::write_supported;
    using glz::from;
    using glz::to;
    using glz::parse;
    using glz::serialize;
    using glz::JSON;
    using glz::BEVE;
    using glz::get;
    using glz::set;

    using glz::reflect;
    using glz::reflectable;
    using glz::for_each_field;
    using glz::name_v;
    using glz::get_enum_name;
    using glz::glaze_object_t;
    using glz::string_literal;

    using glz::stencil;
}
