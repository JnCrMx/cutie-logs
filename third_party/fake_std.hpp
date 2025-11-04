#pragma once

#include <cstdint>
#include <filesystem>
#include <ostream>

_LIBCPP_BEGIN_NAMESPACE_STD
        namespace filesystem {
            inline bool exists( const std::filesystem::path& p, std::error_code& ec ) noexcept {
                __builtin_trap();
            }
            inline bool exists( const std::filesystem::path& p ) {
                __builtin_trap();
            }
            inline std::uintmax_t file_size( const std::filesystem::path& p, std::error_code& ec ) noexcept {
                __builtin_trap();
            }
            inline bool create_directory( const std::filesystem::path& p, std::error_code& ec ) noexcept {
                __builtin_trap();
            }
            inline bool create_directory( const std::filesystem::path& p ) {
                __builtin_trap();
            }

            class directory_entry {
                public:
                    bool is_directory() const {
                        __builtin_trap();
                    }
                    bool is_regular_file() const {
                        __builtin_trap();
                    }
                    std::filesystem::path path() const {
                        __builtin_trap();
                    }
            };

            class directory_iterator {
                public:
                    directory_iterator() noexcept {
                        __builtin_trap();
                    }
                    explicit directory_iterator(const std::filesystem::path& p) {
                        __builtin_trap();
                    }

                    bool operator==(const directory_iterator&) const {
                        __builtin_trap();
                    }
                    bool operator!=(const directory_iterator&) const {
                        __builtin_trap();
                    }
                    directory_iterator& operator++() {
                        __builtin_trap();
                    }
                    const std::filesystem::directory_entry& operator*() const {
                        __builtin_trap();
                    }
            };
            inline directory_iterator begin(directory_iterator iter) {
                __builtin_trap();
            }
            inline directory_iterator end(const directory_iterator&) {
                __builtin_trap();
            }
        }

        template<>
        class basic_ofstream<char> : public std::basic_ostream<char> {
            public:
                basic_ofstream(const char* filename, ios_base::openmode mode) {
                    __builtin_trap();
                }
        };
_LIBCPP_END_NAMESPACE_STD
