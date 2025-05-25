module;

#include <cpr/cpr.h>

export module cpr;

export namespace cpr {
    using cpr::Get;
    using cpr::Post;
    using cpr::Put;
    using cpr::Delete;
    using cpr::Patch;
    using cpr::Head;
    using cpr::Options;

    using cpr::GetAsync;
    using cpr::PostAsync;
    using cpr::PutAsync;
    using cpr::DeleteAsync;
    using cpr::PatchAsync;
    using cpr::HeadAsync;
    using cpr::OptionsAsync;

    using cpr::GetCallback;
    using cpr::PostCallback;
    using cpr::PutCallback;
    using cpr::DeleteCallback;
    using cpr::PatchCallback;
    using cpr::HeadCallback;
    using cpr::OptionsCallback;

    using cpr::AsyncResponse;
    using cpr::AsyncWrapper;
    using cpr::Authentication;
    using cpr::Body;
    using cpr::Cookies;
    using cpr::Header;
    using cpr::Multipart;
    using cpr::Parameters;
    using cpr::Payload;
    using cpr::Response;
    using cpr::Session;
    using cpr::Timeout;
    using cpr::Timeout;
    using cpr::Url;
}
