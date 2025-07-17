module frontend.translations;

import std;

namespace frontend {

constexpr char en_data[] = {
    #embed "en.gmo"
};
constexpr std::span<const char> en{en_data, sizeof(en_data)};

constexpr char de_data[] = {
    #embed "de.gmo"
};
constexpr std::span<const char> de{de_data, sizeof(de_data)};

constexpr char pl_data[] = {
    #embed "pl.gmo"
};
constexpr std::span<const char> pl{pl_data, sizeof(pl_data)};

std::expected<mo_file, mo_file::error> load_translation(std::string_view lang) {
    if(lang == "en") {
        return mo_file::create(en);
    } else if(lang == "de") {
        return mo_file::create(de);
    } else if(lang == "pl") {
        return mo_file::create(pl);
    }
    return std::unexpected(mo_file::error::not_found);
}

}
