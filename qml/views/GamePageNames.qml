import QtQuick

// What each game page choice is called, for the settings page and the right-click menus.
// See docs/code/qml.md.
QtObject {
    readonly property var fields: ({
        steam_waiting: qsTr("Steam changes waiting"),
        steam_update: qsTr("Steam update"),
        steam_family: qsTr("Steam Family"),
        last_backup: qsTr("Last backup"),
        protondb: qsTr("ProtonDB"),
        friends_playing: qsTr("Friends who play"),
        play_time: qsTr("Play time"),
        last_played: qsTr("Last played"),
        recent_play_time: qsTr("Recent play time"),
        times_played: qsTr("Times played"),
        first_played: qsTr("First played"),
        release_date: qsTr("Release date"),
        platform: qsTr("Platform"),
        status: qsTr("Status"),
        install_size: qsTr("Install size"),
        rating: qsTr("Rating"),
        date_added: qsTr("Date added")
    })

    readonly property var buttons: ({
        favorite: qsTr("Favorite"),
        open_platform: qsTr("Open in Steam or the emulator"),
        folder: qsTr("Open folder"),
        save_folder: qsTr("Save folder"),
        settings: qsTr("Properties"),
        gallery: qsTr("Gallery"),
        mod_manager: qsTr("Mod manager"),
        menu: qsTr("Menu")
    })

    readonly property var sections: ({
        description: qsTr("Description"),
        tags: qsTr("Tags"),
        clips: qsTr("Steam media")
    })

    function forPlace(place) {
        return place === "buttons" ? buttons : place === "sections" ? sections : fields
    }
}
