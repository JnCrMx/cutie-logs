module;
#include <cstdint>
#include <string>
#include <fstream>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <vector>
#include <filesystem>

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

namespace backend::utils {

#if defined(__amd64__) || defined(__x86_64) || defined(__x86_64__)
    export constexpr std::string_view host_arch = "amd64";
#elif defined(__arm) || defined(__arm__)
    export constexpr std::string_view host_arch = "arm32";
#elif defined(__aarch64__)
    export constexpr std::string_view host_arch = "arm64";
#elif false // no idea how to detect it
    export constexpr std::string_view host_arch = "ia64";
#elif (defined(__powerpc__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__)) && !(defined(__powerpc64__) || defined(__PPC64__))
    export constexpr std::string_view host_arch = "ppc32";
#elif defined(__powerpc64__) || defined(__PPC64__)
    export constexpr std::string_view host_arch = "ppc64";
#elif defined(__s390__) || defined(__s390x__)
    export constexpr std::string_view host_arch = "s390x";
#elif defined(__i386) || defined(__i386__)
    export constexpr std::string_view host_arch = "x86";
#elif defined(__riscv)
    export constexpr std::string_view host_arch = "riscv64";
#else
    export constexpr std::string_view host_arch = "unknown";
    #warning Failed to detect CPU architecture
#endif

#if defined(__linux) || defined(__linux__)
    export constexpr std::string_view os_type = "linux";
#else
    export constexpr std::string_view os_type = "unknown";
    #warning Failed to detect operating system type
#endif

export std::unordered_map<std::string, std::string> get_os_release() {
    std::fstream f("/etc/os-release", std::ios::in);
    if(!f) {
        return {};
    }
    std::unordered_map<std::string, std::string> map;
    std::string line;
    while(std::getline(f, line)) {
        std::istringstream iss(line);
        std::string key, value;
        if(!std::getline(iss, key, '=')) {
            continue;
        }
        if(!(iss >> std::quoted(value))) {
            continue;
        }
        map[std::move(key)] = std::move(value);
    }
    return map;
}

export std::vector<std::string> get_process_command_args() {
    std::fstream f("/proc/self/cmdline", std::ios::in);
    if(!f) {
        return {};
    }
    std::vector<std::string> args{};
    std::string element;
    while(std::getline(f, element, '\0')) {
        args.push_back(std::move(element));
    }
    return args;
}

}
