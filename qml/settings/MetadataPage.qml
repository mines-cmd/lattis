import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property string description: qsTr("Your keys for looking up games' info on IGDB and their art on SteamGridDB.")

    readonly property string currentSection: ""
    function jumpTo(section) {}
    function jumpBy(step) {}
    function focusSearch() {}
    function clearSearch() { return false }

    Component.onCompleted: {
        missing.reload()
        Metadata.reloadKeys()
        igdbId.text = Metadata.keys.igdbClientId
        igdbSecret.text = Metadata.keys.igdbClientSecret
        gridKey.text = Metadata.keys.steamGridDbKey
    }

    signal lookUpMissing()

    // How many games lack something; counted when the page shows, and again when its window
    // comes back (after "Look up missing info").
    BulkLookupModel { id: missing }
    onVisibleChanged: if (visible) missing.reload()
    Connections {
        target: root.Window.window
        function onActiveChanged() {
            if (root.visible && root.Window.window.active)
                missing.reload()
        }
    }

    property var testing: ({})
    property var tested: ({})

    function test(service) {
        save.saveNow()
        const next = Object.assign({}, testing)
        next[service] = Metadata.testKeys(service)
        testing = next
        const done = Object.assign({}, tested)
        delete done[service]
        tested = done
    }

    Connections {
        target: Metadata
        function onFinished(request, result) {
            for (const service in root.testing) {
                if (root.testing[service] !== request)
                    continue
                const next = Object.assign({}, root.testing)
                delete next[service]
                root.testing = next
                const done = Object.assign({}, root.tested)
                done[service] = result.error
                root.tested = done
            }
        }
    }

    Timer {
        id: save
        interval: 500
        onTriggered: flush()

        function saveNow() {
            stop()
            flush()
        }

        function flush() {
            const fields = [["igdb", "client_id", igdbId], ["igdb", "client_secret", igdbSecret],
                            ["steamgriddb", "api_key", gridKey]]
            const stored = [Metadata.keys.igdbClientId, Metadata.keys.igdbClientSecret, Metadata.keys.steamGridDbKey]
            for (let i = 0; i < fields.length; ++i) {
                const [section, key, field] = fields[i]
                if (field.text.trim() !== stored[i]) {
                    error.text = Metadata.setKey(section, key, field.text)
                    if (error.text !== "")
                        return
                }
            }
        }
    }

    component Card: Rectangle {
        default property alias rows: column.data
        Layout.fillWidth: true
        implicitHeight: column.implicitHeight + (Theme.settingsWindow.cardPadding + Theme.settingsWindow.rowPadding) * 2
        radius: Theme.settingsWindow.cardRadius
        color: Theme.settingsWindow.cardBackground
        border.color: Theme.settingsWindow.cardBorder
        border.width: Theme.settingsWindow.cardBorderWidth

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: Theme.settingsWindow.cardPadding + Theme.settingsWindow.rowPadding
            spacing: Theme.layout.spacing
        }
    }

    component Muted: Label {
        color: Theme.controls.descriptionColor
        font.pixelSize: Theme.controls.descriptionFontSize
        wrapMode: Text.WordWrap
    }

    readonly property real smallButtonWidth: Math.max(showMetric.implicitWidth, hideMetric.implicitWidth,
                                                      testMetric.implicitWidth)
    SettingsButton { id: showMetric; visible: false; text: qsTr("Show") }
    SettingsButton { id: hideMetric; visible: false; text: qsTr("Hide") }
    SettingsButton { id: testMetric; visible: false; text: qsTr("Test") }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        spacing: Theme.layout.spacing

        Label {
            id: error
            visible: text !== ""
            Layout.fillWidth: true
            color: Theme.controls.problemsText
            font.pixelSize: Theme.controls.descriptionFontSize
            wrapMode: Text.WordWrap
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: content.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: SettingsScrollBar { id: scrollBar }

            ColumnLayout {
                id: content
                width: flick.width - scrollBar.width - Theme.layout.spacing
                spacing: 0

                HelpSectionHeader {
                    text: qsTr("IGDB")
                    description: qsTr("Titles, descriptions, release years and ratings.")
                    helpText: qsTr("How to get these")
                    helpUrl: "https://api-docs.igdb.com/#account-creation"
                    // Broken into lines: tooltips only wrap at the window's width.
                    helpTip: qsTr("IGDB uses a Twitch developer app. Register one with any name,\n"
                                  + "OAuth redirect URL http://localhost and client type Confidential,\n"
                                  + "then copy its client ID and a new secret here.")
                }
                Card {
                    KeyRow {
                        id: igdbRow
                        label: qsTr("Client ID")
                        buttonWidth: root.smallButtonWidth
                        onEdited: save.restart()
                        onFinished: save.saveNow()
                    }
                    KeyRow {
                        id: secretRow
                        label: qsTr("Client secret")
                        secret: true
                        buttonWidth: root.smallButtonWidth
                        onEdited: save.restart()
                        onFinished: save.saveNow()
                    }
                    ConnectionRow {
                        buttonWidth: root.smallButtonWidth
                        busy: root.testing.igdb !== undefined
                        result: root.tested.igdb
                        onTest: root.test("igdb")
                        ready: igdbId.text.trim() !== "" && igdbSecret.text.trim() !== ""
                    }
                }

                HelpSectionHeader {
                    text: qsTr("SteamGridDB")
                    description: qsTr("Covers, heroes, logos and icons. Any SteamGridDB account can make an API key.")
                    helpText: qsTr("How to get one")
                    helpUrl: "https://www.steamgriddb.com/profile/preferences/api"
                }
                Card {
                    KeyRow {
                        id: gridRow
                        label: qsTr("API key")
                        secret: true
                        buttonWidth: root.smallButtonWidth
                        onEdited: save.restart()
                        onFinished: save.saveNow()
                    }
                    ConnectionRow {
                        buttonWidth: root.smallButtonWidth
                        needsKeysText: qsTr("Add the key above to test it")
                        busy: root.testing.steamgriddb !== undefined
                        result: root.tested.steamgriddb
                        onTest: root.test("steamgriddb")
                        ready: gridKey.text.trim() !== ""
                    }
                }

                HelpSectionHeader {
                    text: qsTr("Missing info")
                    description: qsTr("Find the descriptions, years, ratings and art your games don't have yet, on IGDB "
                                      + "and SteamGridDB.")
                }
                Card {
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing

                            RowLayout {
                                spacing: 6

                                Icon {
                                    visible: missing.missingCount === 0
                                    path: Icons.check
                                    size: Theme.controls.iconSize
                                    color: Theme.settingsWindow.installFound
                                }
                                Label {
                                    visible: missing.missingCount > 0
                                    text: missing.missingCount
                                    color: Theme.controls.labelColor
                                    font.pixelSize: Theme.settingsWindow.sectionHeaderFontSize
                                    font.weight: Theme.font.strongWeight
                                }
                                Label {
                                    text: missing.missingCount === 0 ? qsTr("Every game has its info")
                                        : missing.missingCount === 1 ? qsTr("game needs info")
                                        : qsTr("games need info")
                                    color: Theme.controls.labelColor
                                    font.pixelSize: Theme.controls.labelFontSize
                                }
                            }
                            Flow {
                                visible: missing.missingCount > 0
                                Layout.fillWidth: true
                                spacing: 6

                                Repeater {
                                    model: [
                                        { key: "description", name: qsTr("Description"), tint: Theme.settingsWindow.missingDescription },
                                        { key: "year", name: qsTr("Year"), tint: Theme.settingsWindow.missingYear },
                                        { key: "rating", name: qsTr("Rating"), tint: Theme.settingsWindow.missingRating },
                                        { key: "art", name: qsTr("Art"), tint: Theme.settingsWindow.missingArt },
                                    ]
                                    delegate: Rectangle {
                                        required property var modelData
                                        readonly property int count: missing.missingCounts[modelData.key] ?? 0
                                        readonly property color tint: modelData.tint

                                        visible: count > 0
                                        implicitWidth: chipText.implicitWidth + Theme.settingsWindow.sourceTagPadding * 2
                                        implicitHeight: chipText.implicitHeight + 2
                                        radius: Theme.settingsWindow.sourceTagRadius
                                        color: Qt.alpha(tint, Theme.settingsWindow.sourceTagFillOpacity)
                                        border.color: tint
                                        border.width: Theme.settingsWindow.sourceTagBorderWidth

                                        Label {
                                            id: chipText
                                            anchors.centerIn: parent
                                            text: modelData.name + "  " + count
                                            color: parent.tint
                                            font.pixelSize: Theme.settingsWindow.sourceTagFontSize
                                        }
                                    }
                                }
                            }
                        }
                        SettingsButton {
                            Layout.alignment: Qt.AlignTop
                            text: qsTr("Look up missing info")
                            tip: Actions.tip(qsTr("Choose games to look up, then review what was found"), Actions.keys.look_up_missing)
                            enabled: missing.missingCount > 0
                            onClicked: root.lookUpMissing()
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.settingsWindow.rowPadding / 2
                        Layout.bottomMargin: Theme.settingsWindow.rowPadding / 2
                        implicitHeight: 1
                        color: Theme.settingsWindow.rowDivider
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Look up new games")
                                color: Theme.controls.labelColor
                                font.pixelSize: Theme.controls.labelFontSize
                            }
                            Muted {
                                Layout.fillWidth: true
                                text: qsTr("New ROMs, Steam games and Steam shortcuts are filled in when IGDB has a clear match, with "
                                           + "SteamGridDB's first image of each kind of art.")
                            }
                            Muted {
                                id: scanError
                                Layout.fillWidth: true
                                visible: text !== ""
                                color: Theme.controls.problemsText
                            }
                        }
                        SettingsSwitch {
                            Layout.alignment: Qt.AlignTop
                            checked: ScanLookup.enabled
                            onToggled: scanError.text = ScanLookup.setEnabled(checked)
                        }
                    }
                }

                Muted {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.settingsWindow.sectionSpacing
                    text: qsTr("Only game titles, platforms and the services' game ids are sent, and only when you click "
                               + "Look up or new games are added. The keys are kept in credentials.toml in your "
                               + "data folder, readable only by you.")
                }
            }
        }
    }

    readonly property alias igdbId: igdbRow.field
    readonly property alias igdbSecret: secretRow.field
    readonly property alias gridKey: gridRow.field
}
