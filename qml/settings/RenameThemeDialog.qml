import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    property string kind: "colors"
    property string name

    readonly property var usage: Theme.usedBy(kind, name)
    readonly property var themes: usage.themes ?? []
    readonly property var files: usage.files ?? []
    readonly property string error: {
        const text = field.text
        const base = Theme.nameError(kind, text)
        if (base !== "")
            return base
        return kind === "theme" && Theme.themeExists(text) ? qsTr("There is already a theme called %1").arg(text) : ""
    }

    signal renamed(string text)

    function openFor(kind, name) {
        root.kind = kind
        root.name = name
        field.text = name
        open()
        field.forceActiveFocus()
        field.selectAll()
    }

    function names(list) {
        if (list.length < 2)
            return list.join("")
        return qsTr("%1 and %2").arg(list.slice(0, -1).join(", ")).arg(list[list.length - 1])
    }

    function rename() {
        const to = field.text
        const ok = kind === "colors" ? Theme.renameColors(name, to)
                 : kind === "style" ? Theme.renameStyle(name, to) : Theme.renameTheme(name, to)
        if (ok)
            renamed(qsTr("Renamed %1 to %2").arg(name).arg(to))
        close()
    }

    anchors.centerIn: parent
    modal: true
    Overlay.modal: Scrim {}
    padding: Theme.menu.dialogPadding
    width: 340

    background: Rectangle {
        color: Theme.menu.popupBackground
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth
        radius: Theme.controls.inputRadius
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: qsTr("Rename %1").arg(root.name)
            color: Theme.controls.labelColor
            font.bold: true
            elide: Text.ElideRight
        }

        Label { text: qsTr("Name"); color: Theme.controls.descriptionColor }
        SettingsTextField {
            id: field
            Layout.fillWidth: true
            implicitHeight: Theme.controls.inputHeight
            onAccepted: if (renameButton.enabled) root.rename()
        }
        Label {
            visible: root.error !== ""
            Layout.fillWidth: true
            text: root.error
            wrapMode: Text.WordWrap
            color: Theme.controls.descriptionColor
        }
        Label {
            visible: root.themes.length > 0 || root.files.length > 0
            Layout.fillWidth: true
            text: root.themes.length > 0 && root.files.length > 0
                  ? qsTr("%1 and %2 use it and follow the new name.").arg(root.names(root.themes)).arg(root.names(root.files))
                  : qsTr("%1 uses it and follows the new name.").arg(root.names(root.themes.length > 0 ? root.themes : root.files))
            wrapMode: Text.WordWrap
            color: Theme.controls.descriptionColor
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.controls.controlSpacing
            SettingsButton { text: qsTr("Cancel"); onClicked: root.close() }
            SettingsButton {
                id: renameButton
                primary: true
                text: qsTr("Rename")
                enabled: root.error === ""
                onClicked: root.rename()
            }
        }
    }
}
