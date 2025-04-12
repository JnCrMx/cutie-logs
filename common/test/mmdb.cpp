#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <variant>
#include <format>

import common;
import glaze;

struct test_struct {
    std::string ip;
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path>" << std::endl;
        return 1;
    }

    std::filesystem::path path = argv[1];
    std::vector<char> data;
    {
        std::ifstream in{path, std::ios::binary | std::ios::ate};
        if (!in) {
            std::cerr << "Failed to open file: " << path << std::endl;
            return 1;
        }
        std::streamsize size = in.tellg();
        in.seekg(0, std::ios::beg);
        data.resize(size);
        if (!in.read(data.data(), size)) {
            std::cerr << "Failed to read file: " << path << std::endl;
            return 1;
        }
    }

    common::mmdb mmdb{std::move(data)};
    if(!mmdb.is_valid()) {
        std::cerr << "Invalid MMDB file: " << mmdb.get_error() << std::endl;
        return 1;
    }

    auto res = mmdb.lookup_v4(0x923470cc);
    if(!res) {
        std::cerr << "Failed to lookup: " << res.error() << std::endl;
        return 1;
    }
    glz::json_t j = res->to_json();

    auto test = common::stencil("{}", j);
    if(!test) {
        std::cerr << "Failed to stencil: " << test.error() << std::endl;
        return 1;
    }
    std::cout << *test << std::endl;

    common::advanced_stencil_functions functions{.m_mmdb = &mmdb};
    test_struct s{.ip = "146.52.112.204"};
    auto test2 = common::stencil("{ip} -> {ip | lookup | at(autonomous_system_organization) }", s, functions);

    if(!test2) {
        std::cerr << "Failed to stencil: " << test2.error() << std::endl;
        return 1;
    }
    std::cout << *test2 << std::endl;
}
