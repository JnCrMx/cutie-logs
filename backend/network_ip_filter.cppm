module;
#include <cstdint>
#include <ranges>
#include <string_view>
#include <vector>
#include <charconv>
#include <expected>

#include <netinet/in.h>

export module backend.utils:network_ip_filter;

import common;
import spdlog;

namespace backend {

export class NetworkIpFilter {
    public:
        enum class filter_action {
            deny,
            allow,
        };

        struct IPv4Subnet {
            uint32_t network;
            int prefix_len;
            filter_action action = filter_action::deny;
        };
        struct IPv6Subnet {
            __uint128_t network;
            int prefix_len;
            filter_action action = filter_action::deny;
        };
    private:
        std::vector<IPv4Subnet> filterlist_ipv4;
        std::vector<IPv6Subnet> filterlist_ipv6;

    public:
        NetworkIpFilter(std::string_view filterlist = "") {
            for(auto rule_ : filterlist | std::views::split(',')) {
                std::string_view rule{rule_};

                if(auto first = rule.find_first_not_of(" \t\r\n"); first != std::string_view::npos) {
                    rule.remove_prefix(first);
                }
                if(auto last = rule.find_last_not_of(" \t\r\n"); last != std::string_view::npos) {
                    rule.remove_suffix(rule.size() - last - 1);
                }
                if(rule.empty()) continue;

                filter_action action = filter_action::deny; // if not action is given, default to block
                int prefix_len = -1; // if no mask is given, block only a single IP address

                if(rule[0] == '+') {
                    action = filter_action::allow;
                    rule.remove_prefix(1);
                } else if(rule[0] == '-') {
                    action = filter_action::deny;
                    rule.remove_prefix(1);
                }

                if(auto slash = rule.find('/'); slash != std::string_view::npos) {
                    std::string_view mask = rule.substr(slash+1);
                    if(std::from_chars(mask.data(), mask.data()+mask.size(), prefix_len).ec != std::errc{}) {
                        throw std::runtime_error("cannot parse IP address mask: " + std::string{mask});
                    }
                    rule = rule.substr(0, slash);
                }

                if(auto ipv4 = common::parse_ipv4(rule)) {
                    if(prefix_len == -1) {
                        prefix_len = 32;
                    } else if(prefix_len > 32 || prefix_len < 0) {
                        throw std::runtime_error("invalid IP address mask: " + std::to_string(prefix_len));
                    }
                    filterlist_ipv4.emplace_back(*ipv4, prefix_len, action);
                } else if(auto ipv6 = common::parse_ipv6(rule)) {
                    if(prefix_len == -1) {
                        prefix_len = 128;
                    } else if(prefix_len > 128 || prefix_len < 0) {
                        throw std::runtime_error("invalid IP address mask: " + std::to_string(prefix_len));
                    }
                    filterlist_ipv6.emplace_back(*ipv6, prefix_len, action);
                } else {
                    throw std::runtime_error("failed to parse ip address: " + std::string{rule});
                }
            }
        }

        bool filter(uint32_t ipv4) {
            for(const auto& subnet : filterlist_ipv4) {
                uint32_t mask = (subnet.prefix_len == 0) ? 0 : (~0U << (32 - subnet.prefix_len));
                if((ipv4 & mask) == (subnet.network & mask)) {
                    switch (subnet.action) {
                        case filter_action::allow: return true;
                        case filter_action::deny: return false;
                    }
                }
            }
            return true;
        }
        bool filter(__uint128_t ipv6) {
            for(const auto& subnet : filterlist_ipv6) {
                __uint128_t mask = (subnet.prefix_len == 0) ? 0 : (~static_cast<__uint128_t>(0U) << (128 - subnet.prefix_len));
                if((ipv6 & mask) == (subnet.network & mask)) {
                    switch (subnet.action) {
                        case filter_action::allow: return true;
                        case filter_action::deny: return false;
                    }
                }
            }
            return true;
        }

        bool filter(sockaddr* address) { // use sockaddr here to be independent of libcurl (so this can also be used for notifications not using cpr)
            if(!address) {
                return false;
            }

            if(address->sa_family == AF_INET) {
                auto* sockaddr = reinterpret_cast<sockaddr_in*>(address);
                uint32_t ip = ntohl(sockaddr->sin_addr.s_addr);
                return filter(ip);
            } else if(address->sa_family == AF_INET6) {
                auto* socketaddr = reinterpret_cast<sockaddr_in6*>(address);
                __uint128_t ip = common::ntoh128(std::bit_cast<__uint128_t>(socketaddr->sin6_addr.s6_addr));
                return filter(ip);
            }

            return false; // fail closed
        }

        const std::vector<IPv4Subnet>& get_ipv4_filterlist() const {
            return filterlist_ipv4;
        }
        const std::vector<IPv6Subnet>& get_ipv6_filterlist() const {
            return filterlist_ipv6;
        }
};

}

export template<>
struct fmt::formatter<backend::NetworkIpFilter::filter_action> : fmt::formatter<std::string>
{
    auto format(backend::NetworkIpFilter::filter_action action, format_context &ctx) const -> decltype(ctx.out())
    {
        switch (action) {
            case backend::NetworkIpFilter::filter_action::deny: return fmt::format_to(ctx.out(), "DENY");
            case backend::NetworkIpFilter::filter_action::allow: return fmt::format_to(ctx.out(), "ALLOW");
            default: return fmt::format_to(ctx.out(), "UNKNOWN");
        }
    }
};

export template<>
struct fmt::formatter<backend::NetworkIpFilter::IPv4Subnet> : fmt::formatter<std::string>
{
    auto format(const backend::NetworkIpFilter::IPv4Subnet& subnet, format_context &ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{} {}/{}", subnet.action, common::ipv4_to_string(subnet.network), subnet.prefix_len);
    }
};

export template<>
struct fmt::formatter<backend::NetworkIpFilter::IPv6Subnet> : fmt::formatter<std::string>
{
    auto format(const backend::NetworkIpFilter::IPv6Subnet& subnet, format_context &ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{} {}/{}", subnet.action, common::ipv6_to_string(subnet.network), subnet.prefix_len);
    }
};
