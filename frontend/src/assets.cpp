module frontend.assets;

import std;

namespace assets {
    namespace icons {
        constexpr char themes_data[] = {
            #embed "assets/icons/themes.svg"
        };
        const std::string_view themes{themes_data, sizeof(themes_data)};

        constexpr char run_data[] = {
            #embed "assets/icons/run.svg"
        };
        const std::string_view run{run_data, sizeof(run_data)};

        constexpr char download_data[] = {
            #embed "assets/icons/download.svg"
        };
        const std::string_view download{download_data, sizeof(download_data)};
    }
}
