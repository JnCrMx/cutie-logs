module;
#if BUILD_TARGET_backend
#include <glaze/net/http_server.hpp>
#include <asio/io_context.hpp>
#endif

export module asio;


#if BUILD_TARGET_backend
export namespace asio {
    using asio::io_context;
    using asio::error_code;
}
export namespace asio::ip {
    using asio::ip::tcp;
    using asio::ip::make_address;
}
#endif
