module;
#include <array>
#include <cstdint>
#include <expected>
#include <ranges>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

export module common:mmdb;
import glaze;

namespace common {

export class mmdb {
    public:
        struct data;
        struct data_cache_container {};
        struct end_marker {};

        enum class ip_version {
            v4 = 4,
            v6 = 6
        };

        using map = std::unordered_map<std::string, data>;
        using array = std::vector<data>;
        using bytes = std::vector<char>;
        using string = std::string;

        using data_base = std::variant<
                                  // pointer (1) is immediately dereferenced
            string,               // UTF-8 string (2)
            double,               // double (3)
            bytes,                // bytes (4)
            uint16_t,             // unsigned 16-bit int (5)
            uint32_t,             // unsigned 32-bit int (6)
            map,                  // map (7)
            int32_t,              // signed 32-bit int (8)
            uint64_t,             // unsigned 64-bit int (9)
            __uint128_t,          // unsigned 128-bit int (10)
            array,                // array (11)
            data_cache_container, // cache container (12)
            end_marker,           // end marker (13)
            bool,                 // boolean (14)
            float                 // float (15)
        >;

        struct data : data_base {
            using data_base::data_base;

            glz::json_t to_json() const {
                return std::visit([](const auto& v) -> glz::json_t {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, string>) {
                        return v;
                    } else if constexpr (std::is_same_v<T, double>) {
                        return v;
                    } else if constexpr (std::is_same_v<T, bytes>) {
                        glz::json_t::array_t arr;
                        arr.reserve(v.size());
                        for (const auto& byte : v) {
                            arr.push_back(static_cast<int>(byte));
                        }
                    } else if constexpr (std::is_same_v<T, map>) {
                        glz::json_t::object_t obj;
                        for (const auto& [key, value] : v) {
                            obj[key] = value.to_json();
                        }
                        return obj;
                    } else if constexpr (std::is_same_v<T, array>) {
                        glz::json_t::array_t arr;
                        arr.reserve(v.size());
                        for (const auto& item : v) {
                            arr.push_back(item.to_json());
                        }
                        return arr;
                    } else if constexpr (std::is_same_v<T, uint16_t>) {
                        return v;
                    } else if constexpr (std::is_same_v<T, uint32_t>) {
                        return v;
                    } else if constexpr (std::is_same_v<T, int32_t>) {
                        return v;
                    } else if constexpr (std::is_same_v<T, uint64_t>) {
                        return v;
                    } else if constexpr (std::is_same_v<T, __uint128_t>) {
                        return static_cast<uint64_t>(v);
                    } else if constexpr (std::is_same_v<T, data_cache_container>) {
                        return glz::json_t::null_t{};
                    } else if constexpr (std::is_same_v<T, end_marker>) {
                        return glz::json_t::null_t{};
                    } else if constexpr (std::is_same_v<T, bool>) {
                        return v;
                    } else if constexpr (std::is_same_v<T, float>) {
                        return v;
                    } else {
                        static_assert(!std::is_same_v<T, T>, "Unsupported type");
                    }
                    return glz::json_t::null_t{};
                }, *this);
            }
        };

        mmdb() {
            m_valid = false;
            m_error = "Empty MMDB";
        }
        mmdb(std::vector<char> data) : m_data(std::move(data)) {
            parse_header();
        }
        mmdb(const mmdb& other) : m_data(other.m_data),
                             m_metadata(other.m_metadata),
                             m_valid(other.m_valid),
                             m_error(other.m_error),
                             m_record_size(other.m_record_size),
                             m_node_count(other.m_node_count),
                             m_tree_size(other.m_tree_size),
                             m_data_begin(other.m_data_begin),
                             m_ip_version(other.m_ip_version)
        {
        }
        mmdb(mmdb&& other) : m_data(std::move(other.m_data)),
                             m_metadata(std::move(other.m_metadata)),
                             m_valid(other.m_valid),
                             m_error(std::move(other.m_error)),
                             m_record_size(other.m_record_size),
                             m_node_count(other.m_node_count),
                             m_tree_size(other.m_tree_size),
                             m_data_begin(other.m_data_begin),
                             m_ip_version(other.m_ip_version)
        {
            other.m_valid = false;
            other.m_error = "Moved MMDB";
        }

        mmdb& operator=(const mmdb& other) {
            if (this == &other) {
                return *this;
            }
            m_data = other.m_data;
            m_metadata = other.m_metadata;
            m_valid = other.m_valid;
            m_error = other.m_error;
            m_record_size = other.m_record_size;
            m_node_count = other.m_node_count;
            m_tree_size = other.m_tree_size;
            m_data_begin = other.m_data_begin;
            m_ip_version = other.m_ip_version;

            return *this;
        }
        mmdb& operator=(mmdb&& other) {
            if (this == &other) {
                return *this;
            }

            m_data = std::move(other.m_data);
            m_metadata = std::move(other.m_metadata);
            m_valid = other.m_valid;
            m_error = std::move(other.m_error);
            m_record_size = other.m_record_size;
            m_node_count = other.m_node_count;
            m_tree_size = other.m_tree_size;
            m_data_begin = other.m_data_begin;
            m_ip_version = other.m_ip_version;

            other.m_valid = false;
            other.m_error = "Moved MMDB";
            return *this;
        }

        const map& get_metadata() const {
            return m_metadata;
        }
        bool is_valid() const {
            return m_valid;
        }
        const std::string& get_error() const {
            return m_error;
        }
        ip_version get_ip_version() const {
            return m_ip_version;
        }
        std::expected<data, std::string> lookup_v6(__uint128_t ip) {
            if(m_ip_version == ip_version::v4) {
                return std::unexpected("Invalid IP version");
            }
            return lookup<__uint128_t>(ip, 0);
        }
        std::expected<data, std::string> lookup_v4(uint32_t ip) {
            if(m_ip_version == ip_version::v6) {
                __uint128_t ip_v6 = ip;
                ip_v6 |= (__uint128_t(0xFFFF) << 32);
                return lookup_v6(ip_v6);
            }
            return lookup<uint32_t>(ip, 0);
        }
    private:
        std::vector<char> m_data;
        map m_metadata;
        bool m_valid = false;
        std::string m_error;
        std::size_t m_record_size{};
        std::size_t m_node_count{};
        std::size_t m_tree_size{};
        std::size_t m_data_begin{};
        ip_version m_ip_version{};

        void parse_header() {
            constexpr std::array magic = {
                '\xab', '\xcd', '\xef', 'M', 'a', 'x', 'M', 'i', 'n', 'd', '.', 'c', 'o', 'm'
            };
            auto res = std::ranges::search(m_data.rbegin(), m_data.rend(), magic.rbegin(), magic.rend());
            if(res.empty()) {
                m_valid = false;
                m_error = "Invalid MMDB file";
                return;
            }
            auto pos = m_data.size() - std::distance(m_data.rbegin(), res.begin());
            auto metadata = read_type(pos, pos);
            if(!metadata) {
                m_valid = false;
                m_error = "Failed to read metadata: " + metadata.error();
                return;
            }
            if(!std::holds_alternative<map>(*metadata)) {
                m_valid = false;
                m_error = "Invalid metadata type";
                return;
            }
            m_metadata = std::move(std::get<map>(*metadata));

            m_ip_version = ip_version{std::get<uint16_t>(m_metadata["ip_version"])};
            m_record_size = std::get<uint16_t>(m_metadata["record_size"]);
            m_node_count = std::get<uint32_t>(m_metadata["node_count"]);
            m_tree_size = ((m_record_size * 2) / 8) * m_node_count;
            m_data_begin = m_tree_size + 16;

            if(m_data_begin >= m_data.size()) {
                m_valid = false;
                m_error = "Invalid data offset";
                return;
            }
            if(m_record_size != 24 && m_record_size != 28 && m_record_size != 32) {
                m_valid = false;
                m_error = "Invalid record size";
                return;
            }

            m_valid = true;
        }

        std::pair<uint32_t, uint32_t> get_node(std::size_t index) {
            std::size_t offset = index * ((m_record_size * 2) / 8);
            if(m_record_size == 24) {
                uint32_t a = *read_uint32(offset, 3);
                uint32_t b = *read_uint32(offset, 3);
                return {a, b};
            } else if(m_record_size == 28) {
                uint32_t a = *read_uint32(offset, 3);
                uint32_t middle = m_data[offset++];
                uint32_t b = *read_uint32(offset, 3);
                uint32_t upper_a = ((middle & 0b11110000) >> 4);
                uint32_t upper_b =  (middle & 0b00011111);

                a = upper_a << 24 | a;
                b = upper_b << 24 | b;
                return {a, b};
            } else if(m_record_size == 32) {
                uint32_t a = *read_uint32(offset, 4);
                uint32_t b = *read_uint32(offset, 4);
            }
            return {m_node_count, m_node_count};
        }

        template<typename T>
        std::expected<data, std::string> lookup(T ip, std::size_t node) {
            if(!m_valid) {
                return std::unexpected(m_error);
            }

            while(ip) {
                auto [a, b] = get_node(node);
                if(ip & (T{1} << (sizeof(T) * 8 - 1))) {
                    node = b;
                } else {
                    node = a;
                }

                if(node == m_node_count) {
                    return std::unexpected("Not found");
                } else if(node > m_node_count + 16) {
                    std::size_t offset = (node - m_node_count) + m_tree_size;
                    return read_type(offset, m_data_begin);
                } else if(node > m_node_count) {
                    return std::unexpected("Invalid node");
                }
                ip <<= 1;
            }
            return std::unexpected("Invalid IP");
        }

        #define CHECK_INDEX(len) \
            if((index + (len - 1)) >= m_data.size()) { /* index is already at the first byte of the new value */ \
                return std::unexpected("Index out of bounds"); \
            }

        std::expected<data, std::string> read_type(std::size_t& index, std::size_t base) {
            CHECK_INDEX(1);
            unsigned char control = static_cast<unsigned char>(m_data[index++]);
            int type = (control & 0b11100000) >> 5;
            if(type == 1) {
                auto ptr = read_pointer(index, control);
                if(!ptr) {
                    return std::unexpected(ptr.error());
                }
                std::size_t target = base + *ptr;
                if(target >= m_data.size()) {
                    return std::unexpected("Pointer out of bounds");
                }
                return read_type(target, base);
            }

            if(type == 0) {
                CHECK_INDEX(1);
                type = static_cast<unsigned char>(m_data[index++]) + 7;
            }

            unsigned int size = (control & 0b00011111);
            if(size == 29) {
                CHECK_INDEX(1);
                size = 29 + m_data[index++];
            } else if(size == 30) {
                CHECK_INDEX(2);
                size = 285 + (m_data[index+1] << 8) + m_data[index + 2];
                index += 2;
            } else if(size == 31) {
                CHECK_INDEX(3);
                size = 65821 + (m_data[index + 1] << 16) + (m_data[index + 2] << 8) + m_data[index + 3];
                index += 3;
            }

            switch(type) {
                case 2:
                    return read_string(index, size);
                case 3:
                    return read_double(index);
                case 4:
                    return read_bytes(index, size);
                case 5:
                    return read_uint16(index, size);
                case 6:
                    return read_uint32(index, size);
                case 7:
                    return read_map(index, size, base);
                case 8:
                    return read_int32(index, size);
                case 9:
                    return read_uint64(index, size);
                case 10:
                    return read_uint128(index, size);
                case 11:
                    return read_array(index, size, base);
                case 12:
                    index += size;
                    return data_cache_container{};
                case 13:
                    index += size;
                    return end_marker{};
                case 14:
                    return size == 1 ? true : false;
                case 15:
                    return read_float(index);
                default:
                    return std::unexpected("Invalid type");
            }
            return {};
        }
        std::expected<std::size_t, std::string> read_pointer(std::size_t& index, unsigned char ctrl) {
            int size = (ctrl & 0b00011000) >> 3;
            uint32_t v = ctrl & 0b00000111;
            if(size == 0) {
                CHECK_INDEX(1);
                uint8_t next = m_data[index++];
                return (v << 8) | next;
            } else if(size == 1) {
                CHECK_INDEX(2);
                uint8_t a = m_data[index++];
                uint8_t b = m_data[index++];
                uint32_t value = (a << 8) | b;
                return ((v << 16) | value) + 2048;
            } else if(size == 2) {
                CHECK_INDEX(3);
                uint8_t a = m_data[index++];
                uint8_t b = m_data[index++];
                uint8_t c = m_data[index++];
                uint32_t value = (a << 16) | (b << 8) | c;
                return ((v << 24) | value) + 2048 + 524288;
            } else if(size == 3) {
                return read_uint32(index, 4);
            }
            return 0;
        }
        std::expected<double, std::string> read_double(std::size_t& index) {
            auto v = read_uint64(index, 8);
            if(!v) {
                return std::unexpected(v.error());
            }
            return std::bit_cast<double>(*v);
        }
        std::expected<float, std::string> read_float(std::size_t& index) {
            auto v = read_uint32(index, 4);
            if(!v) {
                return std::unexpected(v.error());
            }
            return std::bit_cast<float>(*v);
        }
        std::expected<map, std::string> read_map(std::size_t& index, std::size_t len, std::size_t base) {
            std::unordered_map<std::string, data> m;
            for(int i=0; i<len; i++) {
                auto key = read_type(index, base);
                if(!key) { return std::unexpected(key.error()); }

                auto value = read_type(index, base);
                if(!value) { return std::unexpected(value.error()); }

                if (std::holds_alternative<std::string>(*key)) {
                    m[std::get<std::string>(*key)] = std::move(*value);
                } else {
                    return std::unexpected("Invalid key type: " + std::to_string(key->index()));
                }
            }
            return m;
        }
        std::expected<array, std::string> read_array(std::size_t& index, std::size_t len, std::size_t base) {
            std::vector<data> array;
            for(int i=0; i<len; i++) {
                auto e = read_type(index, base);
                if(!e) { return std::unexpected(e.error()); }

                array.push_back(std::move(*e));
            }
            return array;
        }
        std::expected<std::string, std::string> read_string(std::size_t& index, std::size_t len) {
            CHECK_INDEX(len);
            std::string str;
            str.reserve(len);
            for (std::size_t i = 0; i < len; ++i) {
                str.push_back(m_data[index++]);
            }
            return str;
        }
        std::expected<bytes, std::string> read_bytes(std::size_t& index, std::size_t len) {
            CHECK_INDEX(len);
            std::vector<char> bytes;
            bytes.reserve(len);
            for (std::size_t i = 0; i < len; ++i) {
                bytes.push_back(m_data[index++]);
            }
            return bytes;
        }
        std::expected<uint16_t, std::string> read_uint16(std::size_t& index, std::size_t len) {
            CHECK_INDEX(len);
            uint16_t value = 0;
            for (int i = 0; i < len; ++i) {
                value <<= 8;
                value |= static_cast<unsigned char>(m_data[index++]);
            }
            return value;
        }
        std::expected<uint32_t, std::string> read_uint32(std::size_t& index, std::size_t len) {
            CHECK_INDEX(len);
            uint32_t value = 0;
            for (int i = 0; i < len; ++i) {
                value <<= 8;
                value |= static_cast<unsigned char>(m_data[index++]);
            }
            return value;
        }
        std::expected<int32_t, std::string> read_int32(std::size_t& index, std::size_t len) {
            CHECK_INDEX(len);
            int32_t value = 0;
            for (int i = 0; i < len; ++i) {
                value <<= 8;
                value |= static_cast<unsigned char>(m_data[index++]);
            }
            return value;
        }
        std::expected<uint64_t, std::string> read_uint64(std::size_t& index, std::size_t len) {
            CHECK_INDEX(len);
            uint64_t value = 0;
            for (int i = 0; i < len; ++i) {
                value <<= 8;
                value |= static_cast<unsigned char>(m_data[index++]);
            }
            return value;
        }
        std::expected<__uint128_t, std::string> read_uint128(std::size_t& index, std::size_t len) {
            CHECK_INDEX(len);
            __uint128_t value = 0;
            for (int i = 0; i < len; ++i) {
                value <<= 8;
                value |= static_cast<unsigned char>(m_data[index++]);
            }
            return value;
        }
};

}
