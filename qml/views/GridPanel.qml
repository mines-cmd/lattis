import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

// See docs/code/qml.md.
Rectangle {
    id: root

    property var game: ({})
    readonly property bool logoLoading: logo.status === Image.Loading

    signal launchRequested()
    signal stopRequested()
    signal message(string text)
    signal emulatorMessage(string text, string gameId)
    signal editRequested()
    signal menuRequested(Item button)
    signal historyRequested()
    signal customizeRequested()
    signal pageRequested()

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

    Rectangle {
        id: flickFadeMask
        visible: false
        layer.enabled: true
        width: flick.width
        height: flick.height
        gradient: Gradient {
            GradientStop { position: 0; color: flick.fadeTop > 0 ? "transparent" : "white" }
            GradientStop { position: flick.height > 0 ? flick.fadeTop / flick.height : 0; color: "white" }
            GradientStop { position: flick.height > 0 ? 1 - flick.fadeBottom / flick.height : 1; color: "white" }
            GradientStop { position: 1; color: flick.fadeBottom > 0 ? "transparent" : "white" }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.grid.panelPadding
        spacing: Theme.grid.panelSpacing

        Item {
            id: logoBox
            Layout.fillWidth: true
            Layout.topMargin: Theme.grid.panelLogoTopGap
            Layout.bottomMargin: Theme.grid.panelLogoGap
            Layout.preferredHeight: Math.max(Theme.grid.panelLogoMinHeight,
                                             hasLogo ? Math.min(Theme.grid.panelLogoHeight, width * aspect) : titleLabel.implicitHeight)

            readonly property bool hasLogo: logo.status === Image.Ready || logo.status === Image.Loading
            // Another game's logo keeps the old aspect until it has loaded, so the panel doesn't jump twice.
            property real aspect: 0.3

            Connections {
                target: logo
                function onStatusChanged() {
                    if (logo.status === Image.Ready && logo.implicitWidth > 0)
                        logoBox.aspect = logo.implicitHeight / logo.implicitWidth
                }
            }

            Image {
                id: logo
                anchors.centerIn: parent
                width: parent.width
                height: Math.min(parent.height, Theme.grid.panelLogoHeight)
                source: root.game.logoUrl ?? ""
                sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio / 32) * 32, 0)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
            }
            Label {
                id: titleLabel
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
            buttonHeight: Theme.grid.launchHeight
            buttonRadius: Theme.grid.launchRadius
            buttonBorder: Theme.grid.launchBorder
            buttonBorderWidth: Theme.grid.launchBorderWidth
            idleBackground: Theme.grid.launchBackground
            idleBackgroundEnd: Theme.grid.launchBackgroundEnd
            idleText: Theme.grid.launchText
            idleLabel: Theme.grid.launchLabel !== "" ? Theme.grid.launchLabel : Theme.launchButton.label
            buttonFontSize: Theme.grid.launchFontSize
            buttonBold: Theme.grid.launchBold
            onLaunchRequested: root.launchRequested()
            onMessage: text => root.message(text)
            onEmulatorMessage: (text, gameId) => root.emulatorMessage(text, gameId)
            onStopRequested: root.stopRequested()
        }

        RowLayout {
            visible: Theme.grid.showButtons
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.grid.panelButtonsGap
            spacing: Theme.grid.panelSpacing

            SettingsButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                bordered: true
                implicitHeight: Theme.grid.buttonHeight
                cornerRadius: Theme.grid.buttonRadius
                borderColor: Theme.grid.buttonBorder
                borderWidth: Theme.grid.buttonBorderWidth
                fillOverride: Theme.grid.buttonBackground
                textColor: Theme.grid.buttonText
                font.pixelSize: Theme.grid.buttonFontSize
                text: Theme.grid.propertiesLabel
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
                implicitHeight: Theme.grid.buttonHeight
                cornerRadius: Theme.grid.buttonRadius
                borderColor: Theme.grid.buttonBorder
                borderWidth: Theme.grid.buttonBorderWidth
                fillOverride: Theme.grid.buttonBackground
                textColor: Theme.grid.buttonText
                font.pixelSize: Theme.grid.buttonFontSize
                text: Theme.grid.moreLabel
                tip: Actions.tip(qsTr("Game menu"), Actions.keys.game_menu)
                onClicked: root.menuRequested(moreButton)
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.rightMargin: -Theme.grid.panelPadding
            clip: true
            contentWidth: width
            contentHeight: content.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            readonly property real fadeTop: !Theme.grid.edgeFadeTop ? 0
                : Math.min(Theme.grid.panelEdgeFade, Math.max(0, contentY - originY))
            readonly property real fadeBottom: Math.min(Theme.grid.panelEdgeFade,
                Math.max(0, contentHeight + originY - contentY - height))
            layer.enabled: fadeTop > 0 || fadeBottom > 0
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: flickFadeMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1
            }

            ScrollBar.vertical: SettingsScrollBar {
                id: panelBar
                thickness: Theme.gameEditor.bulkScrollbarWidth
                rightPadding: Math.max(0, (Theme.grid.panelPadding - thickness) / 2)
                trackless: true
                handleColor: Theme.list.scrollbarHandle
                handleHoverColor: Theme.list.scrollbarHandleHover
            }

            ColumnLayout {
                id: content
                width: flick.width - Math.max(Theme.grid.panelPadding, panelBar.reserve)
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

                    Surface {
                        visible: chipGrid.strip
                        anchors.fill: chipGrid
                        radius: Theme.statChip.radius
                        fill: Theme.grid.chipBackground
                        fillOpacity: Theme.grid.cardOpacity
                        border.color: Theme.grid.chipBorder
                        border.width: Theme.grid.chipBorderWidth
                    }

                    Repeater {
                        model: chipGrid.strip ? chipRepeater.count : 0

                        Item {
                            required property int index
                            readonly property int column: index % chipGrid.columns
                            readonly property int row: Math.floor(index / chipGrid.columns)
                            readonly property real cellWidth: chipGrid.width / chipGrid.columns
                            readonly property real inset: Theme.grid.stripPadding
                            x: column * cellWidth
                            y: row * Theme.statChip.height
                            width: cellWidth
                            height: Theme.statChip.height

                            Rectangle {
                                visible: parent.column > 0
                                x: -width / 2
                                y: parent.inset
                                width: Theme.grid.stripDividerWidth
                                height: parent.height - 2 * parent.inset
                                color: Theme.grid.stripDivider
                            }
                            Rectangle {
                                visible: parent.row > 0
                                x: parent.inset
                                y: -height / 2
                                width: parent.width - 2 * parent.inset
                                height: Theme.grid.stripDividerWidth
                                color: Theme.grid.stripDivider
                            }
                        }
                    }

                    GridLayout {
                        id: chipGrid
                        width: parent.width
                        readonly property bool strip: Theme.grid.chipStyle === "strip"
                        columns: {
                            const n = chipRepeater.count
                            const gap = strip ? 0 : Theme.grid.panelSpacing
                            const fit = Math.floor((width + gap) / (widestChip + gap))
                            const most = Math.max(1, Math.min(3, fit))
                            // Spread over as few rows as the widest fit allows, then balance them: 4 chips go 2 + 2, not 3 + 1.
                            return Math.max(1, Math.ceil(n / Math.ceil(n / most)))
                        }
                        // Cards fill equal columns; bare chips keep their own width and share the space left between the columns.
                        readonly property bool spread: Theme.grid.chipStyle === "bare"
                        uniformCellWidths: !spread
                        columnSpacing: strip ? 0
                                     : spread && columns > 1 ? Math.max(Theme.grid.panelSpacing, (width - columnsWidth) / (columns - 1))
                                     : Theme.grid.panelSpacing
                        rowSpacing: strip ? 0 : Theme.layout.spacing

                        property real widestChip: 0
                        property var chipWidths: []
                        readonly property real columnsWidth: {
                            let total = 0
                            for (let c = 0; c < columns; ++c) {
                                let widest = 0
                                for (let i = c; i < chipWidths.length; i += columns)
                                    widest = Math.max(widest, chipWidths[i])
                                total += widest
                            }
                            return total
                        }
                        function measureChips() {
                            const widths = []
                            for (let i = 0; i < chipRepeater.count; ++i) {
                                const chip = chipRepeater.itemAt(i)
                                widths.push(chip ? chip.fitWidth : 0)
                            }
                            chipWidths = widths
                            widestChip = Math.max(0, ...widths)
                        }

                        Repeater {
                            id: chipRepeater
                            model: root.chipList
                            onItemAdded: chipGrid.measureChips()
                            onItemRemoved: chipGrid.measureChips()

                            FieldChip {
                                onFitWidthChanged: chipGrid.measureChips()
                                required property string modelData
                                name: modelData
                                carded: Theme.grid.chipStyle === "cards"
                                inStrip: chipGrid.strip
                                fillOpacity: Theme.grid.cardOpacity
                                leftAligned: true
                                // Without a visible surface the padding reads as a misalignment with the buttons above.
                                contentInset: !inStrip && border.width === 0 && fill.a === 0 && fillEnd.a === 0 ? 0 : inStrip ? Theme.grid.stripPadding : Theme.statChip.padding
                                Layout.fillWidth: !chipGrid.spread
                                Layout.preferredWidth: chipGrid.spread ? fitWidth : 1
                                fields: gameFields
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
                        fillOpacity: Theme.grid.cardOpacity
                        fill: Theme.grid.cardBackground
                        fillEnd: Theme.grid.cardBackgroundEnd
                        border.color: Theme.grid.cardBorder
                        border.width: Theme.grid.cardBorderWidth
                        visible: info.rows.length > 0 && General.gamePageGridPanels.includes("info")
                        Layout.row: side.rowOf("info")
                        Layout.fillWidth: true
                        fields: gameFields
                        onHistoryRequested: root.historyRequested()

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: point => root.openPageMenu("grid_panels", info, point.position)
                        }
                    }
                    FriendsPanel {
                        id: friends
                        fillOpacity: Theme.grid.cardOpacity
                        fill: Theme.grid.cardBackground
                        fillEnd: Theme.grid.cardBackgroundEnd
                        border.color: Theme.grid.cardBorder
                        border.width: Theme.grid.cardBorderWidth
                        Layout.row: side.rowOf("friends")
                        Layout.fillWidth: true
                        game: root.game
                        panels: General.gamePageGridPanels
                        onMenuRequested: point => root.openPageMenu("grid_panels", friends, point)
                    }
                    AchievementsPanel {
                        id: achievements
                        fillOpacity: Theme.grid.cardOpacity
                        fill: Theme.grid.cardBackground
                        fillEnd: Theme.grid.cardBackgroundEnd
                        border.color: Theme.grid.cardBorder
                        border.width: Theme.grid.cardBorderWidth
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

        RowLayout {
            Layout.fillWidth: true

            TextLink {
                text: qsTr("Details →")
                onClicked: root.pageRequested()
            }
            Item { Layout.fillWidth: true }
            TextLink {
                visible: !Theme.grid.showButtons
                text: Theme.grid.propertiesLabel
                onClicked: root.editRequested()
                ToolTip.visible: hovered
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: (root.game.pendingSteamEdits ?? false) ? qsTr("Properties — changes are queued until Steam closes")
                                                                     : qsTr("Properties")
            }
        }
    }
}
