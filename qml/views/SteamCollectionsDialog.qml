import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    signal message(string text)

    property var accounts: []
    property string account
    property var read: ({})
    property var ticked: []
    property var summary: null

    readonly property var offered: read.collections ?? []
    readonly property bool anyImported: offered.some(c => c.imported && ticked.includes(c.steamId))

    function toggled(list, value) {
        const next = list.slice()
        const i = next.indexOf(value)
        if (i >= 0)
            next.splice(i, 1)
        else
            next.push(value)
        return next
    }

    function names(rows) {
        return rows.map(r => r.name).join(", ")
    }

    function load() {
        read = account !== "" ? Library.readSteamCollections(account)
                              : { error: qsTr("No Steam account with collections was found") }
        ticked = (read.collections ?? []).map(c => c.steamId)
    }

    function run() {
        const result = Library.importSteamCollections(account, ticked)
        if (result.error !== "") {
            close()
            message(result.error)
            return
        }
        summary = result
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.collections.editWidth, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, parent.height * Theme.gameEditor.maxHeight)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    onAboutToShow: {
        summary = null
        accounts = Library.steamCollectionAccounts()
        account = accounts.length > 0 ? accounts[0].id : ""
        load()
    }
    onOpened: forceActiveFocus()

    component Note: Label {
        Layout.fillWidth: true
        color: Theme.gameEditor.hintColor
        wrapMode: Text.WordWrap
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: root.summary ? qsTr("Imported from Steam") : qsTr("Import collections from Steam")
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            visible: root.summary === null
            Layout.fillWidth: true
            spacing: Theme.gameEditor.rowSpacing

            SettingsComboBox {
                visible: root.accounts.length > 1
                Layout.fillWidth: true
                model: root.accounts.map(a => a.name)
                currentIndex: Math.max(0, root.accounts.findIndex(a => a.id === root.account))
                onActivated: index => {
                    root.account = root.accounts[index].id
                    root.load()
                }
            }

            Note {
                visible: (root.read.error ?? "") !== ""
                text: root.read.error ?? ""
            }
            Note {
                visible: (root.read.error ?? "") === "" && root.offered.length === 0
                text: qsTr("This account has no collections of its own to import.")
            }

            ListView {
                id: checklist
                visible: root.offered.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, Theme.collections.importListHeight)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height
                model: root.offered
                ScrollBar.vertical: SettingsScrollBar { trackless: true }

                delegate: SettingsCheckBox {
                    required property var modelData
                    width: checklist.width
                    bordered: false
                    text: qsTr("%1 (%n game(s))", "", modelData.games).arg(modelData.name)
                          + (modelData.imported ? qsTr(", imported before") : "")
                    checked: root.ticked.includes(modelData.steamId)
                    onToggled: root.ticked = root.toggled(root.ticked, modelData.steamId)
                }
            }

            Note {
                visible: (root.read.dynamic ?? 0) > 0
                text: qsTr("%n collection(s) of Steam's own rules are left out: Lattis can't run them.", "", root.read.dynamic ?? 0)
            }
            Note {
                visible: (root.read.unreadable ?? 0) > 0
                text: qsTr("%n entry(ies) in Steam's file couldn't be read and are skipped.", "", root.read.unreadable ?? 0)
            }
            Note {
                visible: root.anyImported
                text: qsTr("Collections imported before get Steam's games again. Games you added to them in Lattis are removed; their names stay.")
            }
            Note {
                text: qsTr("Nothing is changed in Steam.")
            }
        }

        ColumnLayout {
            id: summaryColumn
            visible: root.summary !== null
            Layout.fillWidth: true
            spacing: Theme.gameEditor.rowSpacing

            readonly property var s: root.summary ?? ({})
            readonly property int changed: (s.added ?? []).length + (s.updated ?? []).length

            Note {
                visible: summaryColumn.changed === 0
                text: qsTr("Nothing needed changing.")
            }
            Note {
                visible: (summaryColumn.s.added ?? []).length > 0
                color: Theme.gameEditor.headingColor
                text: qsTr("Added: %1").arg(root.names(summaryColumn.s.added ?? []))
            }
            Note {
                visible: (summaryColumn.s.updated ?? []).length > 0
                color: Theme.gameEditor.headingColor
                text: qsTr("Updated: %1").arg(root.names(summaryColumn.s.updated ?? []))
            }
            Note {
                visible: (summaryColumn.s.unchanged ?? 0) > 0
                text: qsTr("%n collection(s) already matched Steam.", "", summaryColumn.s.unchanged ?? 0)
            }
            Note {
                visible: (summaryColumn.s.dynamic ?? 0) > 0
                text: qsTr("%n collection(s) of Steam's own rules were left out.", "", summaryColumn.s.dynamic ?? 0)
            }
            Note {
                visible: (summaryColumn.s.unreadable ?? 0) > 0
                text: qsTr("%n entry(ies) in Steam's file couldn't be read and were skipped.", "", summaryColumn.s.unreadable ?? 0)
            }
            Note {
                visible: (summaryColumn.s.missingApps ?? []).length > 0
                text: qsTr("%n game(s) in them aren't in your library; they join their collections if they are added later. Steam's app ids:", "",
                           (summaryColumn.s.missingApps ?? []).length)
            }
            Flickable {
                visible: (summaryColumn.s.missingApps ?? []).length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(missingText.implicitHeight, Theme.collections.importListHeight)
                contentHeight: missingText.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height
                ScrollBar.vertical: SettingsScrollBar { trackless: true }

                Label {
                    id: missingText
                    width: parent.width
                    text: (summaryColumn.s.missingApps ?? []).join(", ")
                    color: Theme.gameEditor.hintColor
                    wrapMode: Text.WordWrap
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                visible: root.summary === null
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            SettingsButton {
                visible: root.summary === null
                text: qsTr("Import")
                enabled: root.ticked.length > 0
                onClicked: root.run()
            }
            SettingsButton {
                visible: root.summary !== null
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
