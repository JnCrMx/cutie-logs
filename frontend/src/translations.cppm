export module frontend.translations;

import std;
import webpp;

namespace frontend {

export class mo_file {
    public:
        enum class error {
            not_open,
            invalid_header,
            unsupported_version,
            not_found,
            out_of_bounds_access,
        };

        mo_file() = default;

        mo_file(const mo_file&) = default;
        mo_file(mo_file&&) = default;

        mo_file& operator=(const mo_file&) = default;
        mo_file& operator=(mo_file&&) = default;

        ~mo_file() = default;

        std::expected<const char*, error> lookup(const char* original) const {
            if(!data.size()) {
                return std::unexpected(error::not_open);
            }

            for(unsigned int i=0; i<number_of_strings; ++i) {
                std::uint32_t original_offset = read_u32(original_table_offset + i * 8 + 4).value();
                std::uint32_t translation_offset = read_u32(translation_table_offset + i * 8 + 4).value();

                if(original_offset >= data.size() || translation_offset >= data.size()) {
                    return std::unexpected(error::out_of_bounds_access);
                }

                const char* original_str = reinterpret_cast<const char*>(&data[original_offset]);
                const char* translation_str = reinterpret_cast<const char*>(&data[translation_offset]);

                if(std::string_view(original_str) == original) {
                    return translation_str;
                }
            }

            return std::unexpected(error::not_found);
        }

        constexpr static std::expected<mo_file, error> create(std::span<const unsigned char> data) {
            if(data.size() < 28) {
                return std::unexpected(error::invalid_header);
            }
            bool little_endian;
            if(read_u32(data, 0, true) == 0x950412de) {
                little_endian = true;
            } else if(read_u32(data, 0, false) == 0x950412de) {
                little_endian = false;
            } else {
                return std::unexpected(error::invalid_header);
            }

            if(read_u32(data, 4, little_endian) != 0) {
                return std::unexpected(error::unsupported_version);
            }
            std::uint32_t number_of_strings = *read_u32(data, 8, little_endian);
            std::uint32_t original_table_offset = *read_u32(data, 12, little_endian);
            std::uint32_t translation_table_offset = *read_u32(data, 16, little_endian);
            std::uint32_t hash_table_size = *read_u32(data, 20, little_endian);
            std::uint32_t hash_table_offset = *read_u32(data, 24, little_endian);

            return mo_file(
                data,
                little_endian,
                number_of_strings,
                original_table_offset,
                translation_table_offset,
                hash_table_size,
                hash_table_offset
            );
        }
    private:
        mo_file(std::span<const unsigned char> data,
                bool little_endian,
                std::uint32_t number_of_strings,
                std::uint32_t original_table_offset,
                std::uint32_t translation_table_offset,
                std::uint32_t hash_table_size,
                std::uint32_t hash_table_offset) :
            data(data),
            little_endian(little_endian),
            number_of_strings(number_of_strings),
            original_table_offset(original_table_offset),
            translation_table_offset(translation_table_offset),
            hash_table_size(hash_table_size),
            hash_table_offset(hash_table_offset) {}

        std::span<const unsigned char> data{};
        bool little_endian;
        std::uint32_t number_of_strings;
        std::uint32_t original_table_offset;
        std::uint32_t translation_table_offset;
        std::uint32_t hash_table_size;
        std::uint32_t hash_table_offset;

        std::expected<std::uint32_t, error> read_u32(std::size_t offset) const {
            return read_u32(data, offset, little_endian);
        }

        constexpr static std::expected<std::uint32_t, error> read_u32(std::span<const unsigned char> data, std::size_t offset, bool little_endian) {
            if (offset + 4 > data.size()) {
                return std::unexpected(error::out_of_bounds_access);
            }
            if(little_endian) {
                return (
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 0])) << 0)  |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 1])) << 8)  |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 2])) << 16) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 3])) << 24)
                );
            } else {
                return (
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 0])) << 24) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 1])) << 16) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 2])) << 8)  |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + 3])) << 0)
                );
            }
        }
};

export std::expected<mo_file, mo_file::error> load_translation(std::string_view lang);

}
