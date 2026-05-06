#include <iostream>
#include <format>

import common;

int main(int argc, const char** argv) {
        if(argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <ip address>" << std::endl;
        return 2;
    }

    std::string_view ip{argv[1]};

    if(auto ipv4 = common::parse_ipv4(ip)) {
        std::cout << std::format("Parsed IPv4: {:08x}\n", *ipv4);
        return 0;
    } else if(auto ipv6 = common::parse_ipv6(ip)) {
        std::cout << std::format("Parsed IPv6: {:032x}\n", *ipv6);
        return 0;
    } else {
        std::cerr << std::format("Errors: IPv4: {}, IPv6: {}\n", static_cast<int>(ipv4.error()), static_cast<int>(ipv6.error()));
        return 2;
    }
}
