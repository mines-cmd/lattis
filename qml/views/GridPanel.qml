import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Rectangle {
    id: root

    property var game: ({})

    signal launchRequested()
    signal stopRequested()
    signal message(string text)
    signal emulatorMessage(string text, string gameId)
    signal editRequested()
    signal menuRequested(Item button)
    signal historyRequested()
    signal steamUndoRequested()
    signal customizeRequested()

    function openInstallDialog() {
        launchButton.openInstallDialog()
    }

    function openMenu() {
        menuRequested(moreButton)
    }

    function openPageMenu(place, item, point) {
        pageMenu.active = true
        pageMenu.item.place = place
        pageMenu.item.popup(item, point.x, point.y)
    }

    function pageMenuForScreenshot(place) {
        const target = place === "grid_chips" ? chipsHost : side
        openPageMenu(place, target, Qt.point(target.width / 2, Math.min(target.height, 40)))
    }

    width: Theme.grid.panelWidth
    color: Theme.grid.panelBackground
    border.color: Theme.grid.panelBorder
    border.width: Theme.grid.panelBorderWidth
    radius: Theme.grid.panelRadius

    GameFields {
        id: gameFields
        game: root.game
        chipNames: General.gamePageGridChips
        updateAction: launchButton.updateAction
        Component.onCompleted: {
            loadLastBackup()
            readPlayed()
        }
    }

    // Another game's chips change in place; the delegates are rebuilt only when the set of chips changes.
    readonly property var shownChips: General.gamePageGridChips.filter(name => gameFields.shown([name]).length > 0)
    property var chipList: []
    onShownChipsChanged: {
        if (chipList.join() !== shownChips.join())
            chipList = shownChips
    }
    Component.onCompleted: chipList = shownChips

    Loader {
        id: pageMenu
        active: false
        source: "GamePageMenu.qml"
    }
    Connections {
        target: pageMenu.item
        function onCustomizeRequested() { root.customizeRequested() }
        function onMessage(text) { root.message(text) }
    }

    SteamPendingMenu {
        id: pendingMenu
        onMessage: text => root.message(text)
        onUndoRequested: root.steamUndoRequested()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.grid.panelPadding
        spacing: Theme.grid.panelSpacing

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.grid.panelLogoHeight

            readonly property bool hasLogo: logo.status === Image.Ready || logo.status === Image.Loading

            Image {
                id: logo
                anchors.fill: parent
                source: root.game.logoUrl ?? ""
                sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio / 32) * 32,
                                    Math.ceil(height * Screen.devicePixelRatio / 32) * 32)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
            }
            Label {
                anchors.fill: parent
                visible: root.game.id !== undefined && !parent.hasLogo
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                text: root.game.title ?? ""
                color: Theme.palette.text
                font.pixelSize: Theme.font.titleSize
                font.weight: Theme.font.strongWeight
            }
        }

        GameLaunchButton {
            id: launchButton
            Layout.fillWidth: true
            game: root.game
            onLaunchRequested: root.launchRequested()
            onMessage: text => root.message(text)
            onEmulatorMessage: (text, gameId) => root.emulatorMessage(text, gameId)
            onStopRequested: root.stopRequested()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.grid.panelSpacing

            SettingsButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                bordered: true
                text: qsTr("Properties")
                tip: Actions.tip((root.game.pendingSteamEdits ?? false) ? qsTr("Properties — changes are queued until Steam closes")
                                                                         : qsTr("Properties"),
                                 Actions.keys.game_settings)
                onClicked: root.editRequested()
            }
            SettingsButton {
                id: moreButton
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                bordered: true
                text: qsTr("More")
                tip: Actions.tip(qsTr("Game menu"), Actions.keys.game_menu)
                onClicked: root.menuRequested(moreButton)
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: content.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar {
                id: panelBar
                thickness: Theme.gameEditor.bulkScrollbarWidth
                trackless: true
                handleColor: Theme.list.scrollbarHandle
                handleHoverColor: Theme.list.scrollbarHandleHover
            }

            ColumnLayout {
                id: content
                width: flick.width - panelBar.reserve
                spacing: Theme.grid.panelSpacing

                Item {
                    id: chipsHost
                    visible: root.chipList.length > 0
                    Layout.fillWidth: true
                    implicitHeight: chipGrid.implicitHeight

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: point => root.openPageMenu("grid_chips", chipsHost, point.position)
                    }

                    GridLayout {
                        id: chipGrid
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: 2
                        columnSpacing: Theme.layout.spacing
                        rowSpacing: Theme.layout.spacing

                        Repeater {
                            model: root.chipList

                            FieldChip {
                                required property string modelData
                                name: modelData
                                fields: gameFields
                                steamMenu: pendingMenu
                                onHistoryRequested: root.historyRequested()
                            }
                        }
                    }
                }

                GridLayout {
                    id: side
                    Layout.fillWidth: true
                    columns: 1
                    rowSpacing: Theme.grid.panelSpacing
                    columnSpacing: 0

                    function rowOf(name) {
                        return Math.max(0, General.gamePageGridPanels.indexOf(name))
                    }

                    InfoPanel {
                        id: info
                        visible: info.rows.length > 0 && General.gamePageGridPanels.includes("info")
                        Layout.row: side.rowOf("info")
                        Layout.fillWidth: true
                        fields: gameFields
                        onHistoryRequested: root.historyRequested()
                        onMessage: text => root.message(text)
                        onUndoRequested: root.steamUndoRequested()

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: point => root.openPageMenu("grid_panels", info, point.position)
                        }
                    }
                    FriendsPanel {
                        id: friends
                        Layout.row: side.rowOf("friends")
                        Layout.fillWidth: true
                        game: root.game
                        panels: General.gamePageGridPanels
                        onMenuRequested: point => root.openPageMenu("grid_panels", friends, point)
                    }
                    AchievementsPanel {
                        id: achievements
                        Layout.row: side.rowOf("achievements")
                        Layout.fillWidth: true
                        game: root.game
                        panels: General.gamePageGridPanels

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: point => root.openPageMenu("grid_panels", achievements, point.position)
                        }
                    }
                    ColumnLayout {
                        id: tags
                        readonly property var list: root.game.tags ?? []
                        visible: list.length > 0 && General.gamePageGridPanels.includes("tags")
                        Layout.row: side.rowOf("tags")
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacingSmall

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: point => root.openPageMenu("grid_panels", tags, point.position)
                        }

                        Label {
                            text: qsTr("Tags")
                            color: Theme.palette.textMuted
                            font.pixelSize: Theme.font.smallSize
                            font.weight: Theme.font.strongWeight
                            font.capitalization: Font.AllUppercase
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.tags.spacing

                            Repeater {
                                model: tags.list

                                TagChip {
                                    id: chip
                                    required property var modelData
                                    name: modelData.name
                                    colorIndex: modelData.color
                                    lit: tagMouse.containsMouse
                                    raised: tagMouse.containsMouse
                                    raiseBy: Theme.details.tagHoverLift
                                    pressed: tagMouse.pressed

                                    MouseArea {
                                        id: tagMouse
                                        readonly property bool filtering: Library.tagFilter.includes(chip.name)
                                        anchors.fill: parent
                                        anchors.bottomMargin: -Theme.details.tagHoverLift
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
                }
            }
        }
    }
}
