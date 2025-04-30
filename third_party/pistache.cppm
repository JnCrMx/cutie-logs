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

            using Pistache::Http::Header::Encoding;
            using Pistache::Http::Header::encodingString;

            using Pistache::Http::Header::Accept;

            using Pistache::Http::Header::Authorization;
            using Pistache::Http::Header::AccessControlAllowMethods;
            using Pistache::Http::Header::AccessControlAllowHeaders;
            using Pistache::Http::Header::AccessControlAllowOrigin;
            using Pistache::Http::Header::ContentType;
            using Pistache::Http::Header::ContentLength;
            using Pistache::Http::Header::ContentEncoding;
            using Pistache::Http::Header::ETag;

            class IfNoneMatch : public Header {
                public:
                    static constexpr const char* Name = "If-None-Match";
                    const char* name() const override { return Name; }
                    static constexpr uint64_t Hash = Pistache::Http::Header::detail::hash(Name);
                    uint64_t hash() const override { return Hash; }

                    IfNoneMatch() = default;

                    void parse(const std::string& value) override {
                        if (value == "*") {
                            m_all = true;
                        } else {
                            m_all = false;
                            std::string etag;
                            std::istringstream stream(value);
                            while (std::getline(stream, etag, ',')) {
                                etag = etag.substr(etag.find_first_not_of(" \t"));
                                m_etags.emplace_back().parse(etag);
                            }
                        }
                    }
                    void write(std::ostream& os) const override {
                        if (m_all) {
                            os << "*";
                        } else {
                            for(unsigned i = 0; i < m_etags.size(); ++i) {
                                m_etags[i].write(os);
                                if (i < m_etags.size() - 1) {
                                    os << ", ";
                                }
                            }
                        }
                    }

                    bool test(std::string_view etag) const {
                        if (m_all) {
                            return false;
                        }
                        for(const auto& e : m_etags) {
                            if(e.etagc() == etag) {
                                return false;
                            }
                        }
                        return true;
                    }
                private:
                    bool m_all;
                    std::vector<ETag> m_etags;
            };
            namespace super_hacky_registration {
                inline Registrar<IfNoneMatch> RegisterIfNoneMatch{};
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
