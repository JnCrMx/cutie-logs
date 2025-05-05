module frontend.assets;

import std;

namespace frontend::assets {
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

        constexpr char settings_data[] = {
            #embed "assets/icons/settings.svg"
        };
        const std::string_view settings{settings_data, sizeof(settings_data)};

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

        constexpr char refresh_data[] = {
            #embed "assets/icons/refresh.svg"
        };
        const std::string_view refresh{refresh_data, sizeof(refresh_data)};

        constexpr char reset_data[] = {
            #embed "assets/icons/reset.svg"
        };
        const std::string_view reset{reset_data, sizeof(reset_data)};

        constexpr char warning_data[] = {
            #embed "assets/icons/warning.svg"
        };
        const std::string_view warning{warning_data, sizeof(warning_data)};

        constexpr char error_data[] = {
            #embed "assets/icons/error.svg"
        };
        const std::string_view error{error_data, sizeof(error_data)};

        constexpr char add_data[] = {
            #embed "assets/icons/add.svg"
        };
        const std::string_view add{add_data, sizeof(add_data)};

        constexpr char remove_data[] = {
            #embed "assets/icons/remove.svg"
        };
        const std::string_view remove{remove_data, sizeof(remove_data)};

        constexpr char delete_data[] = {
            #embed "assets/icons/delete.svg"
        };
        const std::string_view delete_{delete_data, sizeof(delete_data)};

        constexpr char edit_data[] = {
            #embed "assets/icons/edit.svg"
        };
        const std::string_view edit{edit_data, sizeof(edit_data)};
    }
}
