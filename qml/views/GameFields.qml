import QtQuick

// See docs/code/qml.md.
QtObject {
    id: root

    property var game: ({})
    // The Launch button's update state, which tints the Steam update entry.
    property bool updateAction: false

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
            play_time: { label: qsTr("Play time"), value: game.id ? playTime(game.playMinutes) || qsTr("None") : "",
                         icon: Icons.clock, history: true, tip: qsTr("Show play history") },
            last_played: { label: qsTr("Last played"), value: game.id ? game.lastPlayedText || qsTr("Never") : "",
                           icon: Icons.history },
            release_date: { label: qsTr("Release date"), value: game.year ?? "", icon: Icons.calendarEventLine },
            platform: { label: qsTr("Platform"), value: game.platformName ?? "", icon: Icons.forPlatform(game.platformIcon ?? ""),
                        valueIcon: true, filterPlatform: game.platform ?? "",
                        tip: qsTr("Show only %1 games").arg(game.platformName ?? "") },
            status: { label: qsTr("Status"), value: game.statusText ?? "", icon: Icons.flagLine },
            install_size: { label: qsTr("Install size"), value: game.sizeText ?? "", icon: Icons.hardDrive2Line },
            rating: { label: qsTr("Rating"), value: (game.rating ?? -1) >= 0 ? game.rating.toFixed(1) : "", icon: Icons.star,
                      stars: game.rating ?? -1 },
            times_played: { label: qsTr("Times played"), value: played.timesPlayed ?? "", icon: Icons.loopRightLine },
            first_played: { label: qsTr("First played"), value: played.firstPlayed ?? "", icon: Icons.signpostLine },
            recent_play_time: { label: periodLabels[General.recentPeriod] ?? "", value: playTime((played.recent ?? {})[General.recentPeriod]),
                                icon: Icons.timerLine },
            date_added: { label: qsTr("Date added"), value: game.addedText ?? "", icon: Icons.calendarCheckLine },
            steam_waiting: steamWaitingField,
            steam_update: steamUpdateField,
            steam_family: steamFamilyField,
            last_backup: lastBackupField,
            protondb: protonDbField,
            friends_playing: friendsPlayingField
        }
    }

    readonly property bool steamWaitingShown: !Library.readOnly && Library.steamWaiting.length > 0
        && ((game.installed ?? true) || (game.steamUpdate ?? "none") === "none")
    readonly property bool pendingMine: game.pendingSteamEdits ?? false
    readonly property var steamWaitingField: {
        const waiting = Library.steamWaiting.length
        return { label: qsTr("Steam"), icon: Icons.informationLine, attention: true, action: "pending",
                 value: !steamWaitingShown ? ""
                      : (game.shortcutQueued ?? false) ? qsTr("Added when it closes")
                      : pendingMine ? qsTr("Changes wait for it to close")
                      : waiting === 1 ? qsTr("1 game's changes waiting")
                      : qsTr("%1 games' changes waiting").arg(waiting),
                 hoverTip: pendingMine ? "" : qsTr("Waiting for Steam to close: %1").arg(Library.steamWaiting.map(w => w.title).join(", ")),
                 tip: qsTr("Apply the changes now, or undo them") }
    }
    readonly property var steamUpdateField: ({
        label: (game.installed ?? true) ? qsTr("Update available") : qsTr("Installing"),
        value: (game.steamUpdate ?? "none") !== "none" ? game.steamUpdateText ?? "" : "",
        icon: Icons.download, attention: true,
        attentionColor: updateAction ? Theme.launchButton.updateBackground : Theme.launchButton.updatingBackground
    })
    readonly property var steamFamilyField: ({
        label: qsTr("Shared by"), icon: Icons.steamLine,
        value: (game.steamOwnership ?? "") !== "family" ? ""
             : (game.sharedBy ?? []).length > 0 ? game.sharedBy.join(", ")
             : qsTr("Shared with you")
    })

    readonly property bool wantsLastBackup: General.gamePageChips.includes("last_backup")
        || General.gamePageInfoRows.includes("last_backup")
    onWantsLastBackupChanged: loadLastBackup()
    onGameChanged: loadLastBackup()
    function loadLastBackup() {
        if (wantsLastBackup && (game.id ?? "") !== "")
            GameSaves.loadLast(game.id)
    }
    readonly property var lastBackupField: ({
        label: qsTr("Last backup"), icon: Icons.save2Line,
        value: GameSaves.last.requested !== game.id || (GameSaves.last.when ?? "") === "" ? ""
             : GameSaves.last.name ? qsTr("%1, %2").arg(GameSaves.last.when).arg(GameSaves.last.name)
             : GameSaves.last.when
    })

    readonly property int protonDbAppId: {
        const m = /^steam-(\d+)$/.exec(game.id ?? "")
        return m ? parseInt(m[1]) : 0
    }
    readonly property bool protonDbWanted: (General.gamePageChips.includes("protondb")
                                            || General.gamePageInfoRows.includes("protondb")) && protonDbAppId > 0
    // Asks ProtonDb (which starts its request) only while the entry is wanted; the singleton isn't built before.
    property int protonDbRevision: 0
    // A binding, not a value set from change handlers: a handler writing it while protonDbField was being
    // evaluated (a game arriving as the settings page opens) was a binding loop.
    readonly property var protonDbSummary: {
        return protonDbRevision >= 0 && protonDbWanted ? ProtonDb.summary(protonDbAppId) : ({})
    }
    property Connections protonDbWatch: Connections {
        target: root.protonDbWanted ? ProtonDb : null
        function onChanged(appId) {
            if (appId === root.protonDbAppId)
                ++root.protonDbRevision
        }
    }
    readonly property var protonDbField: {
        const summary = protonDbSummary
        const names = {
            native: qsTr("Native"), platinum: qsTr("Platinum"), gold: qsTr("Gold"), silver: qsTr("Silver"),
            bronze: qsTr("Bronze"), borked: qsTr("Borked"), pending: qsTr("Pending")
        }
        const state = summary.state ?? ""
        const tier = state === "ready" ? summary.tier : ""
        const known = names[tier] !== undefined
        const trending = state === "ready" && (summary.trending ?? "") !== tier ? names[summary.trending] ?? "" : ""
        return {
            label: qsTr("ProtonDB"), icon: Icons.gaugeLine, action: "url",
            url: "https://www.protondb.com/app/" + protonDbAppId,
            iconTint: known ? Theme.statChip["protondb" + tier.charAt(0).toUpperCase() + tier.slice(1)] : undefined,
            value: protonDbAppId === 0 ? ""
                 : known ? names[tier]
                 : state === "checking" ? qsTr("Checking…")
                 : state === "none" ? qsTr("No reports")
                 : qsTr("Open page"),
            tip: known ? (trending !== "" ? qsTr("%1 from %2 reports. Trending: %3. Open on ProtonDB")
                                              .arg(names[tier]).arg(summary.total).arg(trending)
                                          : qsTr("%1 from %2 reports. Open on ProtonDB").arg(names[tier]).arg(summary.total))
                       : qsTr("Open on ProtonDB")
        }
    }

    readonly property bool friendsPlayingWanted: protonDbAppId > 0
        && (General.gamePageChips.includes("friends_playing") || General.gamePageInfoRows.includes("friends_playing"))
    property int friendsRevision: 0
    property Connections friendsWatch: Connections {
        target: root.friendsPlayingWanted ? FriendsActivity : null
        function onChanged() { ++root.friendsRevision }
    }
    readonly property int friendsPlayed: friendsRevision >= 0 && friendsPlayingWanted
        ? FriendsActivity.playedCount(protonDbAppId) : 0
    readonly property var friendsPlayingField: ({
        label: qsTr("Friends"), icon: Icons.users,
        value: friendsPlayed > 0 ? qsTr("%1 play").arg(friendsPlayed) : ""
    })

    function playTime(minutes) {
        return Library.playTime(minutes ?? 0, General.playTimeFormat)
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
