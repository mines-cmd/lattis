import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
AppDialog {
    id: root

    carded: true
    size: "large"
    title: summary ? qsTr("Imported from Steam") : qsTr("Import collections from Steam")

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
        ticked = (read.collections ?? []).filter(c => c.games > 0).map(c => c.steamId)
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

    width: Math.min(Theme.collections.editWidth, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, parent.height * Theme.gameEditor.maxHeight)

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

    Note {
        visible: root.summary === null
        text: qsTr("Pick the collections to copy into Lattis. Nothing is changed in Steam.")
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

        CheckList {
            visible: root.offered.length > 0
            Layout.fillWidth: true
            model: root.offered.map(c => ({
                id: c.steamId,
                title: c.name,
                enabled: c.games > 0,
                info: (c.imported ? qsTr("Imported before") + "  ·  " : "")
                      + (c.games > 0 ? qsTr("%n game(s)", "", c.games) : qsTr("Empty"))
            }))
            ticked: root.ticked
            withSelectAll: true
            maxListHeight: Theme.collections.importListHeight
            onToggled: id => root.ticked = root.toggled(root.ticked, id)
            onToggledMany: (ids, on) => root.ticked = on ? root.ticked.concat(ids) : root.ticked.filter(id => !ids.includes(id))
        }

        Note {
            visible: (root.read.dynamic ?? 0) > 0 || (root.read.unreadable ?? 0) > 0
            text: [
                (root.read.dynamic ?? 0) > 0
                    ? qsTr("%n collection(s) of Steam's own rules are left out: Lattis can't run them.", "", root.read.dynamic) : "",
                (root.read.unreadable ?? 0) > 0
                    ? qsTr("%n entry(ies) in Steam's file couldn't be read and are skipped.", "", root.read.unreadable) : ""
            ].filter(t => t !== "").join(" ")
        }
        Note {
            visible: root.anyImported
            text: qsTr("Collections imported before get Steam's games again. Games you added to them in Lattis are removed; their names stay.")
        }
    }

    ColumnLayout {
        id: summaryColumn
        visible: root.summary !== null
        Layout.fillWidth: true
        spacing: Theme.gameEditor.rowSpacing

        readonly property var s: root.summary ?? ({})
        readonly property var added: s.added ?? []
        readonly property var updated: s.updated ?? []
        readonly property var missing: s.missingApps ?? []
        readonly property int changed: added.length + updated.length
        readonly property var rows: {
            const out = []
            if (added.length > 0)
                out.push({ title: qsTr("Added"), count: qsTr("%n collection(s)", "", added.length), detail: root.names(added) })
            if (updated.length > 0)
                out.push({ title: qsTr("Updated"), count: qsTr("%n collection(s)", "", updated.length), detail: root.names(updated) })
            if ((s.unchanged ?? 0) > 0)
                out.push({ title: qsTr("Already matched Steam"), count: qsTr("%n collection(s)", "", s.unchanged), detail: "" })
            if (missing.length > 0)
                out.push({ title: qsTr("Not in your library"), count: qsTr("%n game(s)", "", missing.length),
                           detail: qsTr("They join their collections if they are added later. Steam app ids: %1").arg(missing.join(", ")) })
            return out
        }

        Note {
            text: summaryColumn.changed > 0 ? qsTr("%n collection(s) imported.", "", summaryColumn.changed)
                                            : qsTr("Nothing needed changing.")
        }

        Flickable {
            id: results
            visible: summaryColumn.rows.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, Theme.collections.importListHeight)
            contentHeight: resultsBox.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            ScrollBar.vertical: SettingsScrollBar { id: resultsBar; trackless: true }

            Rectangle {
                id: resultsBox
                width: results.width - (resultsBar.visible ? resultsBar.thickness + Theme.layout.spacing : 0)
                implicitHeight: resultRows.implicitHeight
                radius: Theme.controls.inputRadius
                color: Theme.controls.inputBackground
                border.color: Theme.controls.inputBorder
                border.width: Theme.controls.inputBorderWidth

                ColumnLayout {
                    id: resultRows
                    width: parent.width
                    spacing: 0

                    Repeater {
                        model: summaryColumn.rows

                        ColumnLayout {
                            id: result
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 0

                            Rectangle {
                                visible: result.index > 0
                                Layout.fillWidth: true
                                implicitHeight: 1
                                color: Theme.controls.inputBorder
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.margins: Theme.collections.checklistRowPadding
                                spacing: Theme.layout.spacing / 2

                                RowLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        Layout.fillWidth: true
                                        text: result.modelData.title
                                        color: Theme.gameEditor.headingColor
                                        font.weight: Theme.font.strongWeight
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: result.modelData.count
                                        color: Theme.collections.rowCountColor
                                        font.pixelSize: Theme.controls.labelFontSize
                                    }
                                }
                                Label {
                                    visible: result.modelData.detail !== ""
                                    Layout.fillWidth: true
                                    text: result.modelData.detail
                                    color: Theme.controls.labelColor
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }

        Note {
            visible: (summaryColumn.s.dynamic ?? 0) > 0 || (summaryColumn.s.unreadable ?? 0) > 0
            text: [
                (summaryColumn.s.dynamic ?? 0) > 0
                    ? qsTr("%n collection(s) of Steam's own rules were left out.", "", summaryColumn.s.dynamic) : "",
                (summaryColumn.s.unreadable ?? 0) > 0
                    ? qsTr("%n entry(ies) in Steam's file couldn't be read and were skipped.", "", summaryColumn.s.unreadable) : ""
            ].filter(t => t !== "").join(" ")
        }
    }

    footer: [
        SettingsButton {
            visible: root.summary === null
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            visible: root.summary === null
            primary: true
            text: root.ticked.length > 0 ? qsTr("Import %n collection(s)", "", root.ticked.length) : qsTr("Import")
            enabled: root.ticked.length > 0
            onClicked: root.run()
        },
        SettingsButton {
            visible: root.summary !== null
            primary: true
            text: qsTr("Close")
            onClicked: root.close()
        }
    ]
}
