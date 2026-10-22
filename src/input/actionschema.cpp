#include "actionschema.h"

namespace ActionSchema {
namespace {
constexpr Group kGroups[] = {
    {"list", "Game list", Scope::Main},
    {"views", "Views", Scope::Main},
    {"search", "Search", Scope::Main},
    {"game", "Selected game", Scope::Main},
    {"window", "Main window", Scope::Main},
    {"app", "Everywhere", Scope::App},
    {"settings", "Settings window", Scope::Settings},
};

constexpr Action kActions[] = {
    {"list", "previous_game", "Up", "Previous game"},
    {"list", "next_game", "Down", "Next game"},
    {"list", "page_up", "PgUp", "Up a page"},
    {"list", "page_down", "PgDown", "Down a page"},
    {"list", "first_game", "Home", "First game"},
    {"list", "last_game", "End", "Last game"},
    {"list", "collapse_group", "Left", "Collapse the selected game's platform group"},
    {"list", "expand_group", "Right", "Expand the next collapsed platform group"},
    {"list", "previous_group", "Ctrl+Up", "Start of this group, then the previous group"},
    {"list", "next_group", "Ctrl+Down", "Next platform group"},
    {"list", "extend_previous", "Shift+Up", "Select the game above too"},
    {"list", "extend_next", "Shift+Down", "Select the game below too"},
    {"list", "select_all", "Ctrl+A", "Select every listed game"},
    {"list", "clear_selection", "", "Select only the current game"},

    {"views", "show_all", "Ctrl+1", "All games"},
    {"views", "show_platform_1", "Ctrl+2", "1st platform in the rail"},
    {"views", "show_platform_2", "Ctrl+3", "2nd platform in the rail"},
    {"views", "show_platform_3", "Ctrl+4", "3rd platform in the rail"},
    {"views", "show_platform_4", "Ctrl+5", "4th platform in the rail"},
    {"views", "show_platform_5", "Ctrl+6", "5th platform in the rail"},
    {"views", "show_platform_6", "Ctrl+7", "6th platform in the rail"},
    {"views", "show_platform_7", "Ctrl+8", "7th platform in the rail"},
    {"views", "show_platform_8", "Ctrl+9", "8th platform in the rail"},
    {"views", "next_view", "Ctrl+Tab", "Next entry in the rail"},
    {"views", "previous_view", "Ctrl+Shift+Tab", "Previous entry in the rail"},

    {"search", "focus_search", "Ctrl+F /", "Focus the search box"},
    {"search", "open_filters", "Ctrl+Shift+F", "Filters"},
    {"search", "toggle_favorites_filter", "Ctrl+Shift+D", "Show only favorites / all games"},
    {"search", "cycle_sort", "Ctrl+Shift+S", "Next sort order"},
    {"search", "clear_filters", "Ctrl+Shift+Backspace", "Clear filters"},

    {"game", "launch", "Return Enter", "Launch"},
    {"game", "toggle_favorite", "Ctrl+D", "Add to / remove from favorites"},
    {"game", "open_platform", "Ctrl+O", "Open in Steam or the emulator"},
    {"game", "open_folder", "Ctrl+Shift+O", "Open the game folder"},
    {"game", "open_save_folder", "", "Open the save folder"},
    {"game", "game_settings", "Alt+Return", "Properties"},
    {"game", "play_history", "", "Play history"},
    {"game", "hide_game", "", "Hide / unhide"},
    {"game", "uninstall_game", "", "Uninstall (Steam and Proton games)"},
    {"game", "game_menu", "Menu Shift+F10", "Open the game's menu"},

    {"window", "add_game", "Ctrl+N", "Add a game"},
    {"window", "toggle_fullscreen", "F11", "Fullscreen"},
    {"window", "toggle_rail", "", "Show or hide the rail"},
    {"window", "play_statistics", "F9", "Play statistics"},
    {"window", "look_up_missing", "Ctrl+Shift+L", "Look up missing info"},
    {"window", "app_menu", "F10", "Open the app menu (the logo's)"},
    {"window", "scan_roms", "", "Scan ROM folders now"},
    {"window", "apply_steam_changes", "", "Close Steam and apply the waiting changes"},
    {"window", "setup_guide", "", "Open the setup guide"},
    {"window", "about", "", "About Lattis"},

    {"app", "open_settings", "Ctrl+,", "Open settings"},
    {"app", "show_shortcuts", "F1 Ctrl+/", "Keyboard shortcuts"},
    {"app", "open_data_folder", "", "Open the data folder"},
    {"app", "back_up_library", "", "Back up the library"},
    {"app", "back_up_saves", "", "Back up saves"},
    {"app", "import_backup", "", "Import a backup"},
    {"app", "undo", "Ctrl+Z", "Undo the last change, while its message shows"},
    {"app", "quit", "Ctrl+Q", "Quit"},

    {"settings", "settings_search", "Ctrl+F", "Focus the search box"},
    {"settings", "close_settings", "Ctrl+W", "Close the settings window"},
    {"settings", "previous_section", "Ctrl+PgUp", "Previous section"},
    {"settings", "next_section", "Ctrl+PgDown", "Next section"},
    {"settings", "new_colors", "Ctrl+Shift+N", "New colors"},
    {"settings", "new_style", "Ctrl+Alt+N", "New style"},
    {"settings", "open_colors_file", "Ctrl+E", "Open the colors file"},
    {"settings", "open_style_file", "Ctrl+Shift+E", "Open the style file"},
};
}

std::span<const Group> groups()
{
    return kGroups;
}

std::span<const Action> actions()
{
    return kActions;
}

const Group *groupOf(const Action &action)
{
    for (const Group &g : kGroups) {
        if (g.name == action.group)
            return &g;
    }
    return nullptr;
}
}
