import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    property string kind: "colors"
    property string name
    property bool permanent: false

    readonly property var usage: Theme.usedBy(kind, name)
    readonly property var themes: usage.themes ?? []
    readonly property var files: usage.files ?? []
    readonly property bool active: kind === "colors" ? Theme.activeColors === name
                                  : kind === "style" ? Theme.activeStyle === name : false
    readonly property string fallback: kind === "colors" ? "Ember" : "Soft"
    readonly property string file: (kind === "theme" ? "themes/" : kind === "colors" ? "themes/colors/" : "themes/styles/")
                                   + name + ".toml"
    readonly property string noun: kind === "colors" ? qsTr("colors") : kind === "style" ? qsTr("style") : qsTr("theme")

    signal deleted(string text)

    function openFor(kind, name) {
        root.kind = kind
        root.name = name
        permanent = false
        open()
    }

    function names(list) {
        if (list.length < 2)
            return list.join("")
        return qsTr("%1 and %2").arg(list.slice(0, -1).join(", ")).arg(list[list.length - 1])
    }

    function remove() {
        const ok = kind === "colors" ? Theme.deleteColors(name, permanent)
                 : kind === "style" ? Theme.deleteStyle(name, permanent) : Theme.deleteTheme(name, permanent)
        if (ok) {
            close()
            deleted(permanent ? qsTr("Deleted %1").arg(name) : qsTr("Moved %1 to the trash").arg(name))
        } else if (Theme.needsPermanent()) {
            permanent = true
        } else {
            close()
        }
    }

    anchors.centerIn: parent
    modal: true
    Overlay.modal: Scrim {}
    padding: Theme.menu.dialogPadding
    width: 380

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
            text: root.permanent ? qsTr("Delete %1 for good?").arg(root.name) : qsTr("Delete %1?").arg(root.name)
            color: Theme.controls.labelColor
            font.weight: Theme.font.strongWeight
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            text: root.permanent
                  ? qsTr("There is no trash to move %1 to. Deleting it can't be undone.").arg(root.file)
                  : qsTr("This moves %1 to the trash.").arg(root.file)
            wrapMode: Text.WordWrap
            color: Theme.controls.descriptionColor
        }
        Label {
            visible: root.themes.length > 0
            Layout.fillWidth: true
            text: root.themes.length === 1
                  ? qsTr("Used by the theme %1, which will fall back to %2 until you pick another.")
                        .arg(root.names(root.themes)).arg(root.fallback)
                  : qsTr("Used by the themes %1, which will fall back to %2 until you pick others.")
                        .arg(root.names(root.themes)).arg(root.fallback)
            wrapMode: Text.WordWrap
            color: Theme.controls.descriptionColor
        }
        Label {
            visible: root.files.length > 0
            Layout.fillWidth: true
            text: qsTr("Extended by %1, which will fall back the same way.").arg(root.names(root.files))
            wrapMode: Text.WordWrap
            color: Theme.controls.descriptionColor
        }
        Label {
            visible: root.active
            Layout.fillWidth: true
            text: qsTr("It is in use now; %1 takes its place.").arg(root.fallback)
            wrapMode: Text.WordWrap
            color: Theme.controls.descriptionColor
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.controls.controlSpacing
            SettingsButton { text: qsTr("Cancel"); onClicked: root.close() }
            SettingsButton {
                danger: true
                text: root.permanent ? qsTr("Delete for good") : qsTr("Delete")
                onClicked: root.remove()
            }
        }
    }
}
