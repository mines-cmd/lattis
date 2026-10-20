import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    required property var guide

    spacing: Theme.onboarding.spacing

    property var testing: ({})
    property var tested: ({})

    function testForScreenshot() {
        igdbId.text = "screenshot-id"
        igdbSecret.text = "screenshot-secret"
        test("igdb")
    }

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

    Component.onCompleted: {
        Metadata.reloadKeys()
        igdbId.text = Metadata.keys.igdbClientId
        igdbSecret.text = Metadata.keys.igdbClientSecret
        gridKey.text = Metadata.keys.steamGridDbKey
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
                    const error = Metadata.setKey(section, key, field.text)
                    if (error !== "") {
                        root.guide.message(error)
                        return
                    }
                }
            }
        }
    }

    readonly property real smallButtonWidth: Math.max(showMetric.implicitWidth, hideMetric.implicitWidth,
                                                      testMetric.implicitWidth)
    SettingsButton { id: showMetric; visible: false; text: qsTr("Show") }
    SettingsButton { id: hideMetric; visible: false; text: qsTr("Hide") }
    SettingsButton { id: testMetric; visible: false; text: qsTr("Test") }

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
    SettingsCard {
        panel: true
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
    SettingsCard {
        panel: true
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

    SettingsNote {
        Layout.fillWidth: true
        text: qsTr("Without keys, new games come in with their names only. "
                   + "“Look up missing info” in Settings → Metadata can fill them in later.")
    }

    SettingsCard {
        panel: true
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Look up new games")
                    color: Theme.settingsWindow.labelColor
                    font.pixelSize: Theme.settingsWindow.labelFontSize
                }
                SettingsNote {
                    Layout.fillWidth: true
                    text: qsTr("New ROMs, Steam games and Steam shortcuts are filled in when IGDB has a clear match, "
                               + "with SteamGridDB's first image of each kind of art.")
                }
                SettingsNote {
                    id: scanError
                    Layout.fillWidth: true
                    visible: text !== ""
                    color: Theme.settingsWindow.problemsText
                }
            }
            SettingsSwitch {
                Layout.alignment: Qt.AlignTop
                checked: ScanLookup.enabled
                onToggled: scanError.text = ScanLookup.setEnabled(checked)
            }
        }
    }

    readonly property alias igdbId: igdbRow.field
    readonly property alias igdbSecret: secretRow.field
    readonly property alias gridKey: gridRow.field

    Item { Layout.fillHeight: true }
}
