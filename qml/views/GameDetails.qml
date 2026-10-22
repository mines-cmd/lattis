import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    property var game: ({})
    property bool startedUp: false
    property bool allCollapsed: false
    property real dragReserve: 0
    property real toolbarLeft: -1
    readonly property real noticeWidth: {
        const full = width - Theme.details.marginLeft - marginRight
        return toolbarLeft < 0 ? full : Math.max(0, Math.min(full, toolbarLeft - Theme.layout.spacing - Theme.details.marginLeft))
    }

    // The hero follows the selection after a short delay, so holding an arrow key in the
    // list doesn't decode every image on the way. Text updates immediately.
    property var heroGame: ({})
    readonly property alias shownHeroGame: hero.shownGame

    readonly property bool hasGame: game.id !== undefined
    readonly property bool downloading: hasGame && Theme.downloadBar.show && Library.download.gameId !== undefined
    readonly property bool collapsed: General.downloadBarCollapsed
    property real slide: collapsed ? 1 : 0
    Behavior on slide { enabled: Motion.on; MotionNumber { duration: Motion.duration(Theme.downloadBar.collapseDuration) } }
    // Too narrow for the stat chips beside Launch and the info panel beside the description
    // (details.compact_width). Measured between the wide margins, as the compact right margin
    // widens the content.
    function compactAt(w: real): bool {
        return Theme.details.compactWidth > 0
            && w - Theme.details.marginLeft - Theme.details.marginRight - 2 * Theme.details.padding < Theme.details.compactWidth
    }
    readonly property bool compact: compactAt(width)
    readonly property int marginRight: compact ? Theme.details.compactMarginRight : Theme.details.marginRight
    property string descriptionExpandedFor: ""
    readonly property bool onSteam: game.platform === "steam" || game.platform === "proton"

    signal launchRequested()
    signal stopRequested()
    signal message(string text)
    signal emulatorMessage(string text, string gameId)
    signal clicked()
    signal editRequested()
    signal addRequested()
    signal steamUndoRequested()
    signal historyRequested()

    function logoForScreenshot(mode) {
        if (mode === "adjust")
            hero.startAdjust()
        else if (mode.startsWith("scrim"))
            hero.openScrimMenu(mode === "scrim/side")
        else
            hero.openMenu()
    }

    component TextBlock: ColumnLayout {
        property var game: ({})
        property int maxLines: 2147483647
        property bool cut: false
        property bool expanded: false
        readonly property alias descriptionHeading: heading
        readonly property alias description: desc
        readonly property alias moreLink: more
        readonly property alias tagsHeading: tagsTitle
        readonly property alias tagsFlow: flow
        readonly property alias tagRepeater: tags
        // A switch fades the content (the description and Show more) and leaves the Description
        // heading, which is always in the same place, on screen. The tags section goes with the
        // content unless both games have tags: then it stays, and morphs.
        property real contentOpacity: 1
        property real contentShift: 0
        property real descriptionHeadingOpacity: 1
        property real tagsOpacity: contentOpacity
        property real tagsShift: contentShift
        property bool chipsHidden: false

        signal moreClicked()

        spacing: 10

        Label {
            id: heading
            opacity: descriptionHeadingOpacity
            visible: desc.visible
            text: qsTr("Description")
            color: Theme.details.headingColor
            font.weight: Theme.font.strongWeight
            font.pixelSize: Theme.details.headingFontSize
        }
        Label {
            id: desc
            opacity: contentOpacity
            transform: Translate { y: contentShift }
            visible: General.showDescription && text.trim() !== ""
            Layout.fillWidth: true
            text: game.description ?? ""
            color: Theme.details.descriptionColor
            wrapMode: Text.WordWrap
            lineHeight: Theme.details.descriptionLineHeight
            maximumLineCount: maxLines
            elide: Text.ElideRight
        }
        Label {
            id: more
            opacity: contentOpacity
            transform: Translate { y: contentShift }
            visible: desc.visible && cut
            text: expanded ? qsTr("Show less") : qsTr("Show more")
            color: Theme.details.linkColor
            font.underline: moreMouse.containsMouse

            MouseArea {
                id: moreMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: moreClicked()
            }
        }

        Label {
            id: tagsTitle
            opacity: tagsOpacity
            transform: Translate { y: tagsShift }
            visible: General.showTags && tags.count > 0
            Layout.topMargin: desc.visible ? 14 : 0
            text: qsTr("Tags")
            color: Theme.details.headingColor
            font.weight: Theme.font.strongWeight
            font.pixelSize: Theme.details.headingFontSize
        }
        // Another game's tags replace these at once: the switch has its own motion. A tag added to or
        // removed from this game pops in or out.
        KeyedListModel {
            id: tagModel
            leaveSpeed: "fast"
        }
        function showTags() {
            tagModel.scope = game.id
            tagModel.source = (game.tags ?? []).map(t => ({ key: t.name, name: t.name, color: t.color }))
        }
        onGameChanged: showTags()
        Component.onCompleted: showTags()

        Flow {
            id: flow
            // On the Flow, not the chips: the add transition takes over a chip's opacity.
            opacity: chipsHidden ? 0 : tagsOpacity
            transform: Translate { y: tagsShift }
            visible: General.showTags
            Layout.fillWidth: true
            spacing: Theme.tags.spacing

            add: Transition {
                enabled: Motion.on && !tagModel.rebuilt
                MotionNumber { property: "opacity"; from: 0; to: 1 }
                MotionPop { property: "scale"; from: Theme.motion.chipScale; to: 1; speed: "normal" }
            }
            move: Transition {
                enabled: Motion.on && !tagModel.rebuilt
                MotionNumber { properties: "x,y"; speed: "normal" }
            }

            Repeater {
                id: tags
                model: tagModel

                TagChip {
                    id: chip
                    required property var model
                    readonly property bool leaving: model.leaving
                    name: model.name
                    colorIndex: model.color
                    lit: tagMouse.containsMouse
                    raised: tagMouse.containsMouse
                    raiseBy: Theme.details.tagHoverLift
                    pressed: tagMouse.pressed

                    onLeavingChanged: {
                        if (!leaving)
                            return
                        if (Motion.on)
                            leaveAnimation.start()
                        else
                            opacity = 0
                    }
                    PopLeave { id: leaveAnimation; target: chip }

                    MouseArea {
                        id: tagMouse
                        readonly property bool filtering: Library.tagFilter.includes(chip.name)
                        anchors.fill: parent
                        // Down by the lift, so the chip rising doesn't take the pointer off it at the bottom edge.
                        anchors.bottomMargin: -Theme.details.tagHoverLift
                        visible: !chip.leaving
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Library.tagFilter = filtering ? Library.tagFilter.filter(t => t !== chip.name)
                                                                 : Library.tagFilter.concat([chip.name])

                        ToolTip.visible: containsMouse
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: filtering ? qsTr("Stop filtering by %1").arg(chip.name)
                                                : qsTr("Show games tagged %1").arg(chip.name)
                    }
                }
            }
        }
    }

    // The description and tags follow the hero's shown game and switch with the logo. The old content
    // rises away and fades out as soon as another game is picked (`textExit`, over motion.fast), while
    // the hero waits and loads; the new rises into place with the logo, as far and on the same curve.
    // One after the other, so they never overlap; the Description heading stays on screen between
    // them, so it doesn't blink. The exit has its own time: on the hero's curve it was over in a
    // frame or two. When both games have tags, the Tags heading glides to its new place with the logo
    // and the chips morph into the new ones under it.
    property var textGame: ({})
    readonly property bool textPending: textGame.id !== undefined && textGame.id !== game.id
    property bool textExiting: false
    property real textExit: 0
    property real textIn: 1
    property double lastPick: 0
    property string lastPickId: ""
    // Off while a switch is still in flight: picking games quickly only fades, it doesn't rise.
    property bool riseOn: true
    readonly property real riseDistance: riseOn ? Theme.hero.switchRise : 0
    readonly property bool textLeaving: leavingText.game.id !== undefined && textIn < 1
    // The copy's own exit, so a new one starting (textExit back to 0) doesn't bring the old text back.
    property real leavingExit: 1
    readonly property bool tagsMorphing: textLeaving && leavingText.tagsHeading.visible && textColumn.tagsHeading.visible
    readonly property bool tagsStay: textPending && textColumn.tagsHeading.visible && showsTags(game)

    function showsDescription(g): bool {
        return General.showDescription && (g.description ?? "").trim() !== ""
    }
    function showsTags(g): bool {
        return General.showTags && (g.tags ?? []).length > 0
    }

    onGameChanged: {
        if (heroGame.id === undefined || Theme.hero.loadDelay === 0)
            heroGame = game
        else
            heroTimer.restart()

        const now = Date.now()
        const rapid = lastPick > 0 && now - lastPick < Theme.details.rapidSwitch
        if (game.id !== undefined && game.id !== lastPickId) {
            hero.instant = rapid
            lastPick = now
            lastPickId = game.id
        }
        if (rapid && game.id !== undefined && textGame.id !== undefined && textGame.id !== game.id) {
            textExitAnimation.stop()
            textInAnimation.stop()
            leavingExitAnimation.stop()
            textExiting = false
            textExit = 0
            textIn = 1
            leavingText.game = ({})
            textGame = game
            return
        }

        if (game.id !== undefined && textGame.id !== undefined && game.id !== textGame.id)
            riseOn = !(textExiting || textPending || textIn < 1)

        // Not textPending: that binding hasn't seen the new game yet.
        if (textGame.id === undefined || game.id === undefined || textGame.id === game.id
                || Motion.duration(Theme.motion.fast) === 0) {
            textExiting = false
            textExitAnimation.stop()
            textExit = 0
        } else if (!textExiting) {
            textExiting = true
            textExitAnimation.from = 1 - textIn
            textExitAnimation.restart()
        }
    }

    MotionNumber {
        id: textExitAnimation
        target: root
        property: "textExit"
        to: 1
    }
    MotionNumber {
        id: leavingExitAnimation
        target: root
        property: "leavingExit"
        to: 1
    }
    MotionNumber {
        id: textInAnimation
        target: root
        property: "textIn"
        from: 0
        to: 1
        speed: "normal"
    }

    // The new layer's own arrival: the hero's shown layer is already the new one here.
    onShownHeroGameChanged: {
        // A rapid pick already put the text on the selection; the hero catching up with an earlier
        // game must not take it back.
        if (textGame.id !== undefined && textGame.id === game.id && shownHeroGame.id !== game.id)
            return

        const switching = textGame.id !== undefined && shownHeroGame.id !== undefined
                          && textGame.id !== shownHeroGame.id && (hero.shown?.arrival ?? 1) < 1
        if (switching) {
            leavingText.x = textColumn.x
            leavingText.y = textColumn.y
            leavingText.width = textColumn.width
            leavingText.game = textGame
            leavingText.maxLines = textColumn.maxLines
            leavingText.cut = textColumn.cut
            leavingText.expanded = textColumn.expanded
            leavingExit = textExit
            leavingExitAnimation.duration = Motion.duration(Theme.motion.fast) * (1 - textExit)
            leavingExitAnimation.restart()
        } else {
            leavingText.game = ({})
        }
        textExiting = false
        textInAnimation.stop()
        textIn = switching ? 0 : 1
        textGame = shownHeroGame
        if (switching)
            textInAnimation.start()
    }

    property string openingId: ""

    Timer {
        id: openingTimer
        interval: Theme.iconButton.busyTimeout * 1000
        onTriggered: root.openingId = ""
    }
    Connections {
        target: GameRunner
        function onEmulatorExited(gameId) {
            if (gameId === root.openingId)
                root.openingId = ""
        }
    }
    Connections {
        target: root.Window.window
        function onActiveChanged() {
            if (!root.Window.window.active)
                root.openingId = ""
        }
    }

    Timer {
        id: heroTimer
        interval: Theme.hero.loadDelay
        onTriggered: root.heroGame = root.game
    }

    readonly property var actions: ({
        favorite: {
            icon: Icons.star,
            color: (game.favorite ?? false) ? Theme.iconButton.iconActive : Theme.iconButton.iconInactive,
            tip: Actions.tip((game.favorite ?? false) ? qsTr("Remove from favorites") : qsTr("Add to favorites"),
                             Actions.keys.toggle_favorite),
            badge: false
        },
        open_platform: {
            icon: Icons.openExternal,
            color: Theme.iconButton.icon,
            tip: Actions.tip(onSteam ? qsTr("Open in Steam") : qsTr("Open emulator"), Actions.keys.open_platform),
            badge: false
        },
        folder: {
            icon: Icons.folder,
            color: Theme.iconButton.icon,
            tip: Actions.tip(qsTr("Open game folder"), Actions.keys.open_folder),
            badge: false
        },
        settings: {
            icon: Icons.gear,
            color: Theme.iconButton.icon,
            tip: Actions.tip((game.pendingSteamEdits ?? false) ? qsTr("Properties — changes are queued until Steam closes")
                                                               : qsTr("Properties"),
                             Actions.keys.game_settings),
            badge: game.pendingSteamEdits ?? false
        }
    })

    function activate(button, other) {
        const g = other ?? game
        if (g.id === undefined)
            return
        if (button === "favorite") {
            const error = Library.setGameValue(g.id, "favorite", !(g.favorite ?? false))
            if (error !== "")
                message(error)
            return
        }
        let text = actions[button].message ?? ""
        if (button === "open_platform") {
            text = GameRunner.openEmulator(g.id)
            if (text !== "") {
                emulatorMessage(text, g.id)
            } else {
                openingId = g.id
                openingTimer.restart()
            }
            return
        }
        if (button === "settings") {
            editRequested()
            return
        }
        if (button === "folder")
            text = GameRunner.openFolder(g.id)
        if (text !== "")
            message(text)
    }

    readonly property var shownChips: General.gamePageChips.filter(name => stats[name] !== undefined)
    readonly property var stats: ({
        play_time: { icon: Icons.clock, label: qsTr("Play time"), value: game.playTimeText ?? "" },
        last_played: { icon: "", label: qsTr("Last played"), value: game.lastPlayedText ?? "" }
    })

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(Theme.details.messageWidth, parent.width - Theme.details.marginLeft - Theme.details.marginRight)
        visible: !root.hasGame && !Library.loading
        // Fades in; gone at once, so it never lingers over a game.
        opacity: visible ? 1 : 0
        spacing: Theme.layout.spacing

        Behavior on opacity { enabled: Motion.fades && root.startedUp; MotionNumber { fade: true; speed: "normal" } }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: Library.gameCount === 0 ? qsTr("Your library is empty")
                : root.allCollapsed ? qsTr("All groups are collapsed")
                : Library.search !== "" ? qsTr("No games match \"%1\"").arg(Library.search)
                : Library.filtersActive ? qsTr("No games match the filters")
                : Library.notInstalledHidden > 0 ? qsTr("No installed games")
                : qsTr("No games match")
            color: Theme.palette.textMuted
            font.pixelSize: Theme.font.headingSize
        }
        Label {
            visible: Library.gameCount === 0
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            textFormat: Text.StyledText
            text: qsTr("Add a ROM file, or set up your platforms' ROM folders in <b>Settings → Platforms</b> "
                       + "to find all your games.")
            color: Theme.palette.textMuted
        }
        SettingsButton {
            visible: Library.gameCount === 0
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Add a game")
            iconPath: Icons.add
            primary: true
            onClicked: root.addRequested()
        }
        Label {
            visible: Library.gameCount > 0 && Library.notInstalledHidden > 0 && !root.allCollapsed
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("%n game(s) that aren't installed would match.", "", Library.notInstalledHidden)
            color: Theme.palette.textMuted
        }
        RowLayout {
            visible: Library.gameCount > 0 && !root.allCollapsed && (Library.filtersActive || Library.notInstalledHidden > 0)
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.layout.spacing

            SettingsButton {
                visible: Library.filtersActive
                text: qsTr("Clear filters")
                primary: true
                onClicked: Library.clearFilters()
            }
            SettingsButton {
                visible: Library.notInstalledHidden > 0
                text: qsTr("Show not installed")
                onClicked: Library.installedOnly = false
            }
        }
    }

    NoticeBar {
        visible: Library.readOnly
        opacity: visible ? 1 : 0
        z: 1
        x: Theme.details.marginLeft
        y: Theme.layout.margin
        width: root.noticeWidth
        height: Theme.layout.toolbarHeight
        text: Library.readOnlyNotice

        Behavior on opacity { enabled: Motion.fades && root.startedUp; MotionNumber { fade: true; speed: "normal" } }
    }

    // After an Import, below that one when both show. Loaded after the first frame: Backups is
    // made on first use.
    Loader {
        id: importNotice
        active: root.startedUp
        z: 1
        x: Theme.details.marginLeft
        y: Theme.layout.margin + (Library.readOnly ? Theme.layout.toolbarHeight + Theme.layout.spacing : 0)
        width: root.noticeWidth
        height: Theme.layout.toolbarHeight
        source: "ImportNotice.qml"
    }
    Connections {
        target: importNotice.item
        function onMessage(text) { root.message(text) }
    }

    Flickable {
        id: flick

        anchors.fill: parent
        visible: root.hasGame
        clip: true
        contentWidth: width
        contentHeight: content.y + content.implicitHeight + Theme.details.marginBottom
            + (downloading ? (collapsed ? Theme.downloadBar.pillHeight : Theme.downloadBar.height) + Theme.details.spacing : 0)
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        MouseArea {
            width: flick.width
            height: Math.max(flick.contentHeight, flick.height)
            onClicked: root.clicked()
        }

        // While adjusting the logo, a click anywhere off the hero finishes, and does nothing else,
        // like a click off a popup: Launch or a tag under it would be a surprise.
        MouseArea {
            z: 1
            // Hidden, not just disabled: Qt took the pointer's cursor from this MouseArea, over every
            // hand cursor on the page. While it shows, it takes the adjuster's cursor over the logo as well.
            visible: hero.adjusting
            cursorShape: hero.adjusterCursor
            width: flick.width
            height: Math.max(flick.contentHeight, flick.height)
            onPressed: mouse => {
                const p = mapToItem(hero, mouse.x, mouse.y)
                mouse.accepted = !hero.contains(p)
            }
            onClicked: hero.finishAdjust()
        }

        ColumnLayout {
            id: content

            x: Theme.details.marginLeft + Theme.details.padding
            y: Theme.layout.margin + Theme.layout.toolbarHeight + Theme.layout.spacing
            // One expression, not root.marginRight: a binding on it would see compact's old value for a
            // moment, and the hero would load its first game at that width.
            width: root.width - Theme.details.marginLeft - 2 * Theme.details.padding
                   - (root.compactAt(root.width) ? Theme.details.compactMarginRight : Theme.details.marginRight)
            spacing: Theme.details.spacing

            Hero {
                id: hero
                readonly property real artHeight: Math.min(content.width / Theme.hero.aspect, Theme.hero.maxHeight)
                Layout.fillWidth: true
                Layout.preferredHeight: logoOnly ? Theme.hero.logoOnlyHeight : artHeight
                game: root.heroGame
                logoOnly: artHeight < Theme.hero.minHeight
                onMessage: text => root.message(text)
            }

            RowLayout {
                visible: root.game.hidden ?? false
                Layout.fillWidth: true
                spacing: Theme.details.actionSpacing

                Icon {
                    path: Icons.hidden
                    size: Theme.iconButton.iconSize
                    color: Theme.details.descriptionColor
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Hidden. It's listed because hidden games are shown.")
                    color: Theme.details.descriptionColor
                }
                SettingsButton {
                    text: qsTr("Unhide")
                    onClicked: {
                        const error = Library.unhideGame(root.game.id)
                        if (error !== "")
                            root.message(error)
                    }
                }
            }

            // The action row. When it all fits: Launch, the stats, then the buttons at the right
            // end. Otherwise Launch with the buttons beside it (or under it, when even they don't
            // fit), then the stats. Measured, not details.compact_width: the chips' widths vary.
            GridLayout {
                id: actionRow

                readonly property bool oneLine: content.width >= launchButton.implicitWidth + statsFlow.lineWidth
                    + buttons.implicitWidth + 2 * columnSpacing
                readonly property bool buttonsBeside: oneLine
                    || content.width >= Theme.launchButton.height * 2 + columnSpacing + buttons.implicitWidth
                // Off one line, Launch takes the spare width only while it has to shrink; otherwise the
                // buttons' column does, so the chips below, spanning both columns, get the whole row.
                readonly property bool launchShrinks: !buttonsBeside
                    || content.width < launchButton.implicitWidth + columnSpacing + buttons.implicitWidth

                Layout.fillWidth: true
                // The grid's maximum is the sum of its columns' (Launch and the buttons are capped), which
                // would shrink the row, and the chips under them, to that.
                Layout.maximumWidth: Number.POSITIVE_INFINITY
                columnSpacing: Theme.details.actionSpacing
                rowSpacing: Theme.details.actionSpacing

                LaunchButton {
                    id: launchButton
                    Layout.row: 0
                    Layout.column: 0
                    Layout.fillWidth: !actionRow.oneLine && actionRow.launchShrinks
                    Layout.maximumWidth: implicitWidth
                    Layout.minimumWidth: !actionRow.oneLine ? Math.min(implicitWidth, Theme.launchButton.height * 2) : implicitWidth
                    installed: root.game.installed ?? true
                    updateState: root.game.steamUpdate ?? "none"
                    rom: root.game.rom ?? false
                    gameNotFound: root.game.gameNotFound ?? false
                    installable: root.game.platform === "steam"
                    locatable: root.game.platform === "proton" || root.game.platform === "linux"
                    running: GameRunner.running.includes(root.game.id ?? "")
                    tip: (root.game.launchTip ?? "") !== "" ? Actions.tip(root.game.launchTip, Actions.keys.launch) : ""
                    onClicked: root.launchRequested()
                    onStopRequested: root.stopRequested()
                }

                Flow {
                    id: statsFlow
                    // The chips on one line, for the one-line row (a Flow's own implicit width
                    // follows the width it was last given).
                    readonly property real lineWidth: {
                        let w = 0
                        let n = 0
                        for (let i = 0; i < stats.count; ++i, ++n)
                            w += stats.itemAt(i)?.implicitWidth ?? 0
                        if (steamUpdate.visible) {
                            w += steamUpdate.implicitWidth
                            ++n
                        }
                        if (steamPending.visible) {
                            w += steamPending.implicitWidth
                            ++n
                        }
                        return w + Math.max(0, n - 1) * spacing
                    }

                    visible: root.shownChips.length > 0 || steamPending.visible || steamUpdate.visible
                    Layout.row: !actionRow.oneLine ? (actionRow.buttonsBeside ? 1 : 2) : 0
                    Layout.column: !actionRow.oneLine ? 0 : 1
                    Layout.columnSpan: !actionRow.oneLine && actionRow.buttonsBeside ? 3 : 1
                    Layout.fillWidth: !actionRow.oneLine
                    Layout.preferredWidth: !actionRow.oneLine ? 0 : lineWidth
                    Layout.minimumWidth: !actionRow.oneLine ? 0 : lineWidth
                    spacing: Theme.details.actionSpacing

                    Repeater {
                        id: stats
                        model: root.shownChips

                        StatChip {
                            required property string modelData
                            icon: root.stats[modelData].icon
                            label: root.stats[modelData].label
                            value: root.stats[modelData].value
                            clickable: modelData === "play_time"
                            tip: clickable ? qsTr("Show play history") : ""
                            onClicked: root.historyRequested()
                        }
                    }

                    StatChip {
                        id: steamPending
                        readonly property int waiting: Library.steamWaiting.length
                        readonly property bool mine: root.game.pendingSteamEdits ?? false
                        readonly property bool installing: !(root.game.installed ?? true) && (root.game.steamUpdate ?? "none") !== "none"

                        visible: !Library.readOnly && waiting > 0 && !installing
                        attention: true
                        icon: Icons.info
                        label: qsTr("Steam")
                        value: (root.game.shortcutQueued ?? false) ? qsTr("Added when it closes")
                             : mine ? qsTr("Changes wait for it to close")
                             : waiting === 1 ? qsTr("1 game's changes waiting")
                             : qsTr("%1 games' changes waiting").arg(waiting)
                        ToolTip.visible: chipHover.hovered && !mine
                        ToolTip.text: qsTr("Waiting for Steam to close: %1").arg(Library.steamWaiting.map(w => w.title).join(", "))
                        ToolTip.delay: 500

                        HoverHandler { id: chipHover }

                        SettingsButton {
                            id: pendingButton
                            implicitHeight: Theme.statChip.buttonHeight
                            flat: true
                            leftPadding: text === "" ? 0 : 14
                            rightPadding: leftPadding
                            implicitWidth: text === "" ? implicitHeight : contentItem.implicitWidth + leftPadding + rightPadding
                            text: GameRunner.closingSteam ? qsTr("Closing Steam") : ""
                            iconPath: GameRunner.closingSteam ? "" : Icons.chevron
                            iconRotation: pendingMenu.showing ? -90 : 90
                            enabled: !GameRunner.closingSteam
                            tip: qsTr("Apply the changes now, or undo them")
                            onClicked: pendingMenu.popup(pendingButton, 0, pendingButton.height)

                            SettingsMenu {
                                id: pendingMenu

                                Action {
                                    text: qsTr("Apply")
                                    onTriggered: {
                                        const e = GameRunner.closeSteamAndApply()
                                        if (e !== "")
                                            root.message(e)
                                    }
                                }
                                Action {
                                    text: qsTr("Undo")
                                    onTriggered: root.steamUndoRequested()
                                }
                            }
                        }
                    }

                    StatChip {
                        id: steamUpdate
                        visible: (root.game.steamUpdate ?? "none") !== "none"
                        attention: true
                        attentionColor: launchButton.updateAction ? Theme.launchButton.updateBackground
                                                                  : Theme.launchButton.updatingBackground
                        icon: Icons.download2Line
                        label: (root.game.installed ?? true) ? qsTr("Update available") : qsTr("Installing")
                        value: root.game.steamUpdateText ?? ""
                    }
                }

                Item {
                    visible: actionRow.buttonsBeside
                    Layout.row: 0
                    Layout.column: 2
                    Layout.fillWidth: actionRow.oneLine || !actionRow.launchShrinks
                }

                RowLayout {
                    id: buttons
                    Layout.row: actionRow.buttonsBeside ? 0 : 1
                    Layout.column: actionRow.oneLine ? 3 : actionRow.buttonsBeside ? 1 : 0
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                    spacing: Theme.details.buttonSpacing

                    Repeater {
                        model: General.gamePageButtons.filter(b => root.actions[b] !== undefined && (b !== "open_platform" || root.game.platform !== "linux"))

                        IconButton {
                            required property string modelData
                            icon: root.actions[modelData].icon
                            iconColor: root.actions[modelData].color
                            tip: root.actions[modelData].tip
                            badge: root.actions[modelData].badge
                            busy: modelData === "open_platform" && root.openingId !== "" && root.openingId === root.game.id
                            onClicked: root.activate(modelData)
                        }
                    }
                }
            }

            GridLayout {
                id: body
                Layout.fillWidth: true
                columns: root.compact ? 1 : 2
                columnSpacing: Theme.details.columnSpacing
                rowSpacing: Theme.details.spacing

                TextBlock {
                    id: textColumn
                    readonly property real exitOpacity: 1 - root.textExit
                    readonly property real flowOpacity: root.textPending ? exitOpacity : root.textIn
                    readonly property real flowShift: root.textPending ? -root.riseDistance * root.textExit
                                                                       : root.riseDistance * (1 - root.textIn)
                    contentOpacity: Theme.details.animateDescription ? flowOpacity : 1
                    contentShift: Theme.details.animateDescription ? flowShift : 0
                    descriptionHeadingOpacity: root.textPending ? (root.showsDescription(root.game) ? 1 : exitOpacity)
                        : root.textLeaving && leavingText.descriptionHeading.visible ? 1 : root.textIn
                    tagsOpacity: !Theme.details.animateTags ? 1 : root.tagsMorphing || root.tagsStay ? 1 : flowOpacity
                    tagsShift: !Theme.details.animateTags ? 0 : root.tagsMorphing ? (leavingText.tagsHeading.y - tagsHeading.y) * (1 - root.textIn)
                             : root.tagsStay ? 0 : flowShift
                    chipsHidden: root.tagsMorphing
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    game: root.textGame
                    maxLines: needsCut && !expanded ? shownLines : 2147483647
                    cut: needsCut
                    expanded: root.descriptionExpandedFor === (root.textGame.id ?? "")
                    onMoreClicked: root.descriptionExpandedFor = expanded ? "" : root.textGame.id

                    // details.fit_description: how many lines of the description fit the window.
                    // Worked out from the whole text (fullText) and the heights of everything else,
                    // never from the description as shown, which the result changes.
                    readonly property real room: {
                        let others = 0
                        for (const item of [descriptionHeading, tagsHeading, tagsFlow]) {
                            if (item.visible)
                                others += item.implicitHeight + item.Layout.topMargin + spacing
                        }
                        return flick.height - content.y - Theme.details.marginBottom - body.y - others
                            - (root.compact && info.visible ? body.rowSpacing + info.implicitHeight : 0)
                    }
                    readonly property real lineHeightPx: fullText.lineCount > 0 ? fullText.contentHeight / fullText.lineCount : 1
                    readonly property int fittingLines: Math.max(Theme.details.descriptionMinLines,
                        Math.floor((room - moreLink.implicitHeight - spacing) / lineHeightPx))
                    // One fewer when the last line kept is the blank one between two paragraphs,
                    // so "…" doesn't sit on a line of its own.
                    readonly property int shownLines: fittingLines > 1 && fullText.blankLines.includes(fittingLines - 1)
                                                      ? fittingLines - 1 : fittingLines
                    readonly property bool needsCut: Theme.details.fitDescription && fullText.contentHeight > room
                                                     && fullText.lineCount > fittingLines

                    Text {
                        id: fullText
                        visible: false
                        width: textColumn.description.width
                        text: textColumn.description.text
                        font: textColumn.description.font
                        wrapMode: textColumn.description.wrapMode
                        lineHeight: textColumn.description.lineHeight

                        property var blankLines: []
                        onLineLaidOut: line => {
                            const blank = line.number === 0 ? [] : blankLines.slice()
                            if (line.implicitWidth === 0)
                                blank.push(line.number)
                            blankLines = blank
                        }
                    }
                }
                InfoPanel {
                    id: info
                    visible: rows.length > 0
                    Layout.fillWidth: root.compact
                    Layout.preferredWidth: root.compact ? -1 : Theme.infoPanel.width
                    Layout.alignment: Qt.AlignTop
                    game: root.game
                }
            }
        }

        Item {
            x: content.x + body.x
            y: content.y + body.y
            visible: root.textLeaving

            TextBlock {
                id: leavingText
                readonly property real flowOpacity: 1 - root.leavingExit
                contentOpacity: Theme.details.animateDescription ? flowOpacity : 0
                readonly property real flowShift: -root.riseDistance * root.leavingExit
                contentShift: Theme.details.animateDescription ? flowShift : 0
                descriptionHeadingOpacity: textColumn.descriptionHeading.visible ? 0 : 1 - root.leavingExit
                tagsOpacity: Theme.details.animateTags && !root.tagsMorphing ? flowOpacity : 0
                tagsShift: Theme.details.animateTags ? flowShift : 0
                enabled: false
            }

            // Each old chip turns into the new one in its place: it moves, resizes and recolors, and its
            // name crossfades in place, the two opacities always adding up to 1 so the chip never goes empty
            // (faded one after the other, or with the old gone early, there was a moment with no name, which read
            // as blinking).
            // One without a partner fades out or in, moving with the heading.
            Repeater {
                model: root.tagsMorphing ? Math.max(leavingText.tagRepeater.count, textColumn.tagRepeater.count) : 0

                Rectangle {
                    id: morphChip
                    required property int index
                    readonly property Item from: index < leavingText.tagRepeater.count ? leavingText.tagRepeater.itemAt(index) : null
                    readonly property Item to: index < textColumn.tagRepeater.count ? textColumn.tagRepeater.itemAt(index) : null
                    readonly property real p: root.textIn
                    readonly property real glide: textColumn.y + textColumn.tagsHeading.y
                                                  - leavingText.y - leavingText.tagsHeading.y
                    readonly property real toX: to ? textColumn.x + textColumn.tagsFlow.x + to.x : fromX
                    readonly property real toY: to ? textColumn.y + textColumn.tagsFlow.y + to.y : fromY + glide
                    readonly property real fromX: from ? leavingText.x + leavingText.tagsFlow.x + from.x : toX
                    readonly property real fromY: from ? leavingText.y + leavingText.tagsFlow.y + from.y : toY - glide
                    readonly property color fromColor: from ? from.color : to.color
                    readonly property color toColor: to ? to.color : from.color

                    x: fromX + (toX - fromX) * p
                    y: fromY + (toY - fromY) * p
                    width: from && to ? from.width + (to.width - from.width) * p : (from ?? to).width
                    height: Theme.tags.height
                    z: from && to ? 1 : 0
                    opacity: from && to ? 1 : from ? Math.max(0, 1 - 2 * p) : Math.max(0, 2 * p - 1)
                    radius: Theme.tags.radius
                    color: Qt.rgba(fromColor.r + (toColor.r - fromColor.r) * p, fromColor.g + (toColor.g - fromColor.g) * p,
                                   fromColor.b + (toColor.b - fromColor.b) * p, fromColor.a + (toColor.a - fromColor.a) * p)
                    border.color: Theme.tags.border
                    border.width: Theme.tags.borderWidth
                    clip: true

                    Label {
                        anchors.centerIn: parent
                        visible: morphChip.from !== null
                        opacity: morphChip.to ? 1 - morphChip.p : 1
                        text: morphChip.from?.name ?? ""
                        color: morphChip.from ? morphChip.from.fillText : Theme.tags.text
                        font.pixelSize: Theme.tags.fontSize
                        font.weight: morphChip.from ? morphChip.from.nameWeight : Font.Normal
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: morphChip.to !== null
                        opacity: morphChip.from ? morphChip.p : 1
                        text: morphChip.to?.name ?? ""
                        color: morphChip.to ? morphChip.to.fillText : Theme.tags.text
                        font.pixelSize: Theme.tags.fontSize
                        font.weight: morphChip.to ? morphChip.to.nameWeight : Font.Normal
                    }
                }
            }
        }
    }

    Item {
        id: dock
        anchors.fill: parent
        clip: true
        z: 1

        readonly property real pillWidth: downloadPill.item ? downloadPill.item.implicitWidth : 0
        readonly property real morphWidth: content.width + (pillWidth - content.width) * root.slide
        readonly property real morphHeight: Theme.downloadBar.height + (Theme.downloadBar.pillHeight - Theme.downloadBar.height) * root.slide

        readonly property real rowShift: downloadBar.item
            ? Theme.downloadBar.height - downloadBar.item.rowCenter - Theme.downloadBar.pillHeight / 2 : 0

        // The pill sits opaque under the bar and the bar fades away over it, so the card itself never
        // dips in opacity mid-morph.
        Loader {
            id: downloadPill
            active: root.downloading
            visible: root.slide > 0
            x: content.x + content.width - width
            y: root.height - Theme.details.marginBottom - height + dock.rowShift * (1 - root.slide)
            width: dock.morphWidth
            height: dock.morphHeight
            source: "DownloadPill.qml"
        }
        Loader {
            id: downloadBar
            active: root.downloading
            visible: opacity > 0
            opacity: Math.max(0, 1 - root.slide * 1.6)
            x: content.x + content.width - width
            y: root.height - Theme.details.marginBottom - height + dock.rowShift * (1 - root.slide)
            width: dock.morphWidth
            height: dock.morphHeight
            source: "DownloadBar.qml"
        }
    }
    // Whether the pointer was last seen on the chevron, from whichever card is at rest, so the
    // chevron keeps its hover color while the morph hands it from one card to the other.
    property bool chevronHot: false
    Connections {
        target: downloadBar.item
        function onChevronHoveredChanged() {
            if (root.slide <= 0)
                root.chevronHot = downloadBar.item.chevronHovered
        }
    }
    Connections {
        target: downloadPill.item
        function onHoveredChanged() {
            if (root.slide >= 1)
                root.chevronHot = downloadPill.item.hovered
        }
    }
    Binding {
        target: downloadBar.item
        property: "carriedHover"
        value: root.chevronHot && root.slide > 0
        when: downloadBar.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "carriedHover"
        value: root.chevronHot && root.slide < 1
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadBar.item
        property: "morphing"
        value: root.slide > 0
        when: downloadBar.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "slide"
        value: root.slide
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "fullWidth"
        value: content.width
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "iconDrop"
        value: downloadBar.item ? Theme.downloadBar.height / 2 - downloadBar.item.rowCenter : 0
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadBar.item
        property: "download"
        value: Library.download
        when: downloadBar.item !== null
    }
    Binding {
        target: downloadBar.item
        property: "rowFromBottom"
        value: Theme.downloadBar.pillHeight / 2 + dock.rowShift * (1 - root.slide)
        when: downloadBar.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "rowFromBottom"
        value: Theme.downloadBar.pillHeight / 2 + dock.rowShift * (1 - root.slide)
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadBar.item
        property: "tooltips"
        value: !root.collapsed
        when: downloadBar.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "tooltips"
        value: root.slide >= 1
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "download"
        value: Library.download
        when: downloadPill.item !== null
    }
    Connections {
        target: downloadBar.item
        function onMessage(text) { root.message(text) }
        function onCollapse() { General.setDownloadBarCollapsed(true) }
    }
    Connections {
        target: downloadPill.item
        function onExpand() { General.setDownloadBarCollapsed(false) }
    }

    // Drag-to-move zone: the strip above the hero. A plain Item (not MouseArea) so clicks
    // still reach the NoticeBar below; the DragHandler only grabs when a drag starts.
    Item {
        z: 2
        y: 0
        width: parent.width - root.dragReserve
        height: Theme.layout.margin + Theme.layout.toolbarHeight + Theme.layout.spacing

        DragHandler {
            target: null
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.Window.window.startSystemMove()
        }
    }
}
