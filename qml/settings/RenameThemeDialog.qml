import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    carded: false
    title: qsTr("Rename %1").arg(name)

    property string kind: "colors"
    property string name

    readonly property var usage: Theme.usedBy(kind, name)
    readonly property var themes: usage.themes ?? []
    readonly property var files: usage.files ?? []
    property string failure
    // Its own name is taken by itself: nothing to rename yet, and no error.
    readonly property bool unchanged: field.text.trim() === name
    readonly property string error: {
        if (failure !== "")
            return failure
        if (unchanged)
            return ""
        const text = field.text.trim()
        const base = Theme.nameError(kind, text)
        if (base !== "")
            return base
        return kind === "theme" && Theme.themeExists(text) ? qsTr("There is already a theme called %1").arg(text) : ""
    }

    signal renamed(string text)

    function openFor(kind, name) {
        root.kind = kind
        root.name = name
        root.failure = ""
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
        const to = field.text.trim()
        const ok = kind === "colors" ? Theme.renameColors(name, to)
                 : kind === "style" ? Theme.renameStyle(name, to) : Theme.renameTheme(name, to)
        if (!ok) {
            failure = qsTr("Couldn't rename it. The Customize page's problems list says why.")
            return
        }
        renamed(qsTr("Renamed %1 to %2").arg(name).arg(to))
        close()
    }

    Label { text: qsTr("Name"); color: Theme.controls.descriptionColor }
    SettingsTextField {
        id: field
        Layout.fillWidth: true
        implicitHeight: Theme.controls.inputHeight
        maximumLength: Theme.maxNameLength
        onTextChanged: root.failure = ""
        onAccepted: if (renameButton.enabled) root.rename()
    }
    PlainLabel {
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

    footer: [
        SettingsButton { text: qsTr("Cancel"); onClicked: root.close() },
        SettingsButton {
            id: renameButton
            primary: true
            text: qsTr("Rename")
            enabled: !root.unchanged && root.error === ""
            onClicked: root.rename()
        }
    ]
}
