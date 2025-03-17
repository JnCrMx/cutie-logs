module;

#include <cstdint>

#include <pistache/async.h>
#include <pistache/endpoint.h>
#include <pistache/http.h>
#include <pistache/mime.h>
#include <pistache/router.h>

export module pistache;

export namespace Pistache {
    using Pistache::Address;
    using Pistache::IP;
    using Pistache::Ipv4;
    using Pistache::Ipv6;
    using Pistache::Port;

    namespace Http {
        using Pistache::Http::Code;
        using Pistache::Http::Endpoint;
        using Pistache::Http::ResponseWriter;
        using Pistache::Http::Request;
        using Pistache::Http::Method;
        using Pistache::Http::operator<<;

        namespace Header {
            using Pistache::Http::Header::Header;
            using Pistache::Http::Header::Collection;
            using Pistache::Http::Header::Registrar;

            using Pistache::Http::Header::Authorization;
            using Pistache::Http::Header::AccessControlAllowMethods;
            using Pistache::Http::Header::AccessControlAllowHeaders;
            using Pistache::Http::Header::AccessControlAllowOrigin;
            using Pistache::Http::Header::ContentType;

            namespace detail {
                using Pistache::Http::Header::detail::hash;
            }
        }
        namespace Mime {
            using Pistache::Http::Mime::MediaType;
            using Pistache::Http::Mime::Type;
            using Pistache::Http::Mime::Subtype;
        }
    }
    namespace Log {
        using Pistache::Log::Level;
        using Pistache::Log::StringLogger;
        using Pistache::Log::StringToStreamLogger;
    }
    namespace Tcp {
        using Pistache::Tcp::Options;
    }
    namespace Rest {
        using Pistache::Rest::Request;
        using Pistache::Rest::Route;
        using Pistache::Rest::Router;

        namespace Routes {
            using Pistache::Rest::Routes::Delete;
            using Pistache::Rest::Routes::Get;
            using Pistache::Rest::Routes::Head;
            using Pistache::Rest::Routes::NotFound;
            using Pistache::Rest::Routes::Options;
            using Pistache::Rest::Routes::Patch;
            using Pistache::Rest::Routes::Post;
            using Pistache::Rest::Routes::Put;
            using Pistache::Rest::Routes::Remove;

            using Pistache::Rest::Routes::bind;
            using Pistache::Rest::Routes::middleware;
        }
    }
}
