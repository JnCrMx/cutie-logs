module frontend.assets;

import std;

namespace assets {
    namespace icons {
        constexpr char text_view_data[] = {
            #embed "assets/icons/text-view.svg"
        };
        const std::string_view text_view{text_view_data, sizeof(text_view_data)};

        constexpr char table_view_data[] = {
            #embed "assets/icons/table-view.svg"
        };
        const std::string_view table_view{table_view_data, sizeof(table_view_data)};

        constexpr char analysis_data[] = {
            #embed "assets/icons/analysis.svg"
        };
        const std::string_view analysis{analysis_data, sizeof(analysis_data)};

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

        constexpr char warning_data[] = {
            #embed "assets/icons/warning.svg"
        };
        const std::string_view warning{warning_data, sizeof(warning_data)};
    }
}
