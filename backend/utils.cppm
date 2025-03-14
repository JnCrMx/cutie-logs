module;
#include <cstdint>
#include <string>

export module backend.utils;

import spdlog;
import pistache;

export template<>
struct fmt::formatter<Pistache::Address> : fmt::formatter<std::string>
{
    auto format(const Pistache::Address& addr, format_context &ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{}:{}", addr.host(), static_cast<uint16_t>(addr.port()));
    }
};
