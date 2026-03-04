module;
#include <cstdint>
#include <ranges>
#include <string_view>
#include <vector>
#include <charconv>
#include <expected>

#include <netinet/in.h>
#include <curl/curl.h>

#include <iostream>
#include <format>

export module backend.utils:network_ip_filter;

import cpr;
import common;

namespace backend {

export class NetworkIpFilter {
    private:
        static curl_socket_t opensocket_callback(void* clientp, curlsocktype purpose, curl_sockaddr* address) {
            if(purpose == CURLSOCKTYPE_IPCXN) {
                bool accept = static_cast<NetworkIpFilter*>(clientp)->filter(&address->addr);
                if(!accept) {
                    return CURL_SOCKET_BAD;
                }
            }
            return socket(address->family, address->socktype, address->protocol);
        }

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

        std::vector<IPv4Subnet> filterlist_ipv4;
        std::vector<IPv6Subnet> filterlist_ipv6;

    public:
        NetworkIpFilter(std::string_view filterlist = "") {
            for(auto rule_ : filterlist | std::views::split(',')) {
                std::string_view rule{rule_};
                if(rule.empty()) continue;

                filter_action action = filter_action::deny; // if not action is given, default to block
                int prefix_len = 32; // if no mask is given, block only a single IP address

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
                    std::cout << std::format("Parsed IPv4: {}\n", *ipv4);

                    filterlist_ipv4.emplace_back(*ipv4, prefix_len, action);
                } else if(auto ipv6 = common::parse_ipv6(rule)) {
                    std::cout << std::format("Parsed IPv6: {}\n", *ipv6);

                    filterlist_ipv6.emplace_back(*ipv6, prefix_len, action);
                } else {
                    throw std::runtime_error("failed to parse ip address: " + std::string{rule});
                }
            }
        }

        void install(cpr::Session& session) {
            curl_easy_setopt(session.GetCurlHolder()->handle, CURLOPT_OPENSOCKETFUNCTION, opensocket_callback);
            curl_easy_setopt(session.GetCurlHolder()->handle, CURLOPT_OPENSOCKETDATA, this);
        }

        bool filter(sockaddr* address) { // use sockaddr here to be independent of libcurl (so this can also be used for notifications not using cpr)
            if(!address) {
                return false;
            }

            if(address->sa_family == AF_INET) {
                auto* sockaddr = reinterpret_cast<sockaddr_in*>(address);
                uint32_t ip = ntohl(sockaddr->sin_addr.s_addr);

                for(const auto& subnet : filterlist_ipv4) {
                    uint32_t mask = (subnet.prefix_len == 0) ? 0 : (~0U << (32 - subnet.prefix_len));
                    if((ip & mask) == (subnet.network & mask)) {
                        switch (subnet.action) {
                            case filter_action::allow: return true;
                            case filter_action::deny: return false;
                        }
                    }
                }
            } else if(address->sa_family == AF_INET6) {
                auto* socketaddr = reinterpret_cast<sockaddr_in6*>(address);
                __uint128_t ip = std::bit_cast<__uint128_t>(socketaddr->sin6_addr.s6_addr);
                for(const auto& subnet : filterlist_ipv6) {
                    __uint128_t mask = (subnet.prefix_len == 0) ? 0 : (~static_cast<__uint128_t>(0U) << (128 - subnet.prefix_len));
                    if((ip & mask) == (subnet.network & mask)) {
                        switch (subnet.action) {
                            case filter_action::allow: return true;
                            case filter_action::deny: return false;
                        }
                    }
                }
            }

            return true;
        }
};

}
