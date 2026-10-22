import QtQuick

// See docs/code/qml.md.
QtObject {
    property var game: ({})

    readonly property var all: ({
        play_time: { label: qsTr("Play time"), value: game.playTimeText ?? "", icon: Icons.clock, history: true,
                     tip: qsTr("Show play history") },
        last_played: { label: qsTr("Last played"), value: game.lastPlayedText ?? "", icon: "" },
        release_date: { label: qsTr("Release date"), value: game.year ?? "", icon: "" },
        platform: { label: qsTr("Platform"), value: game.platformName ?? "", icon: Icons.forPlatform(game.platformIcon ?? ""),
                    valueIcon: true, filterPlatform: game.platform ?? "",
                    tip: qsTr("Show only %1 games").arg(game.platformName ?? "") },
        status: { label: qsTr("Status"), value: game.statusText ?? "", icon: "" },
        install_size: { label: qsTr("Install size"), value: game.sizeText ?? "", icon: "" },
        rating: { label: qsTr("Rating"), value: (game.rating ?? -1) >= 0 ? game.rating.toFixed(1) : "", icon: Icons.star,
                  stars: game.rating ?? -1 }
    })

    // The names of `names` this game has a value for, in their order.
    function shown(names) {
        return names.filter(name => all[name] !== undefined && all[name].value !== "")
    }
}
