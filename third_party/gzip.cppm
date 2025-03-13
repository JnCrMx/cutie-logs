module;

#include <gzip/compress.hpp>
#include <gzip/decompress.hpp>

export module gzip;

export namespace gzip {
    using gzip::compress;
    using gzip::decompress;
}
