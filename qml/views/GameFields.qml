import QtQuick

// See docs/code/qml.md.
QtObject {
    id: root

    property var game: ({})

    readonly property var periodLabels: ({
        week: qsTr("This week"),
        two_weeks: qsTr("Last 2 weeks"),
        month: qsTr("This month"),
        year: qsTr("This year")
    })

    readonly property var all: {
        // Checked here rather than in a property of its own, which could still hold the previous game's
        // numbers when this binding runs for the new game.
        const played = read.id === (game.id ?? "") ? read : {}
        return {
            play_time: { label: qsTr("Play time"), value: game.id ? game.playTimeText || qsTr("None") : "",
                         icon: Icons.clock, history: true, tip: qsTr("Show play history") },
            last_played: { label: qsTr("Last played"), value: game.id ? game.lastPlayedText || qsTr("Never") : "",
                           icon: Icons.historyFill },
            release_date: { label: qsTr("Release date"), value: game.year ?? "", icon: Icons.calendarEventFill },
            platform: { label: qsTr("Platform"), value: game.platformName ?? "", icon: Icons.forPlatform(game.platformIcon ?? ""),
                        valueIcon: true, filterPlatform: game.platform ?? "",
                        tip: qsTr("Show only %1 games").arg(game.platformName ?? "") },
            status: { label: qsTr("Status"), value: game.statusText ?? "", icon: Icons.flagFill },
            install_size: { label: qsTr("Install size"), value: game.sizeText ?? "", icon: Icons.hardDrive2Fill },
            rating: { label: qsTr("Rating"), value: (game.rating ?? -1) >= 0 ? game.rating.toFixed(1) : "", icon: Icons.star,
                      stars: game.rating ?? -1 },
            times_played: { label: qsTr("Times played"), value: played.timesPlayed ?? "", icon: Icons.loopRightFill },
            first_played: { label: qsTr("First played"), value: played.firstPlayed ?? "", icon: Icons.signpostFill },
            recent_play_time: { label: periodLabels[General.recentPeriod] ?? "", value: (played.recent ?? {})[General.recentPeriod] ?? "",
                                icon: Icons.clock },
            date_added: { label: qsTr("Date added"), value: game.addedText ?? "", icon: Icons.calendarCheckFill }
        }
    }

    // The names of `names` this game has a value for, in their order.
    function shown(names) {
        return names.filter(name => all[name] !== undefined && all[name].value !== "")
    }

    // The history file is read only while a field needs it, and again only when the game's play changed:
    // the game map is replaced on every change to the game (a download's progress among them).
    readonly property var historyFields: ["times_played", "first_played", "recent_play_time"]
    property bool needsHistory: General.gamePageChips.concat(General.gamePageInfoRows)
        .some(name => historyFields.includes(name))
    readonly property string playedKey: needsHistory
        ? [game.id ?? "", game.playTimeText ?? "", game.lastPlayedText ?? ""].join("\n") : ""
    property var read: ({})

    function readPlayed() {
        const id = game.id ?? ""
        const result = needsHistory ? Library.pageHistory(id) : ({})
        // A long history arrives through pageHistoryReady; until then a re-read keeps the numbers it had.
        if (!result.pending)
            read = Object.assign({ id: id }, result)
    }
    onPlayedKeyChanged: readPlayed()

    property Connections historyWatch: Connections {
        target: Library
        function onHistoryChanged(ids) {
            if (root.needsHistory && ids.includes(root.game.id ?? ""))
                root.readPlayed()
        }
        function onPageHistoryReady(id, result) {
            if (root.needsHistory && id === (root.game.id ?? ""))
                root.read = Object.assign({ id: id }, result)
        }
    }
}
