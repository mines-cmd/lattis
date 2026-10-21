import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    property var shortcuts: []
    property var checked: ({})
    property bool addNow: true

    signal done()
    signal chosen(var appIds)

    function openFresh(picked) {
        shortcuts = Library.steamShortcuts()
        const c = {}
        for (const s of shortcuts)
            c[s.appId] = picked ? picked.includes(s.appId) : s.checked
        checked = c
        error.text = ""
        open()
    }

    readonly property var picked: shortcuts.filter(s => checked[s.appId]).map(s => s.appId)

    function add() {
        if (!addNow) {
            chosen(picked)
            close()
            return
        }
        error.text = Library.addSteamShortcuts(picked)
        if (error.text === "") {
            close()
            done()
        }
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.settingsWindow.pickerWidth, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, parent.height - 2 * Theme.gameEditor.padding)
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

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: qsTr("Add existing shortcuts")
            color: Theme.gameEditor.headingColor
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: root.shortcuts.length === 0
                  ? qsTr("Every non-Steam game Steam has is in the library already.")
                  : qsTr("Your non-Steam games in Steam that aren't in the library. They come in as Proton games "
                         + "and keep their shortcut, so their play time, art and Wine prefix stay with them. "
                         + "Uncheck what isn't a game.")
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: contentHeight
            visible: root.shortcuts.length > 0
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.shortcuts
            spacing: Theme.layout.spacing
            ScrollBar.vertical: SettingsScrollBar {}

            delegate: RowLayout {
                id: row
                required property var modelData
                width: ListView.view.width
                spacing: Theme.layout.spacing

                SettingsCheckBox {
                    checked: root.checked[row.modelData.appId] ?? false
                    onToggled: {
                        const c = Object.assign({}, root.checked)
                        c[row.modelData.appId] = checked
                        root.checked = c
                    }
                }
                Rectangle {
                    Layout.preferredWidth: Theme.settingsWindow.pickerArtSize
                    Layout.preferredHeight: Theme.settingsWindow.pickerArtSize
                    radius: Theme.radius.small
                    color: Theme.settingsWindow.cardBackground
                    clip: true
                    Image {
                        anchors.fill: parent
                        visible: row.modelData.art !== ""
                        source: row.modelData.art
                        sourceSize.width: Theme.settingsWindow.pickerArtSize * 2
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.name
                            color: Theme.controls.labelColor
                            font.pixelSize: Theme.controls.labelFontSize
                            elide: Text.ElideRight
                        }
                        Label {
                            text: row.modelData.tool !== "" ? row.modelData.tool : qsTr("runs natively")
                            color: row.modelData.tool !== "" ? Theme.settingsWindow.markerColor
                                                             : Theme.controls.descriptionColor
                            font.pixelSize: Theme.settingsWindow.markerFontSize
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: row.modelData.target
                        color: Theme.controls.descriptionColor
                        font.family: Theme.font.monoFamily
                        font.pixelSize: Theme.controls.descriptionFontSize
                        elide: Text.ElideMiddle
                    }
                }
            }
        }

        Label {
            id: error
            visible: text !== ""
            Layout.fillWidth: true
            color: Theme.controls.problemsText
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                text: root.shortcuts.length === 0 ? qsTr("Close") : qsTr("Cancel")
                onClicked: root.close()
            }
            SettingsButton {
                visible: root.shortcuts.length > 0
                text: root.picked.length === 0 ? qsTr("Add none")
                      : root.picked.length === 1 ? qsTr("Add 1 game") : qsTr("Add %1 games").arg(root.picked.length)
                primary: true
                enabled: root.picked.length > 0 || !root.addNow
                onClicked: root.add()
            }
        }
    }
}
