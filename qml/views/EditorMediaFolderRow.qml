import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: folderRow
    property string path
    property bool steam: false
    property bool exists: true
    signal openClicked()
    signal removeClicked()

    readonly property string home: StandardPaths.writableLocation(StandardPaths.HomeLocation)
                                       .toString().replace(/^file:\/\//, "")
    readonly property int slash: path.lastIndexOf("/")
    readonly property string parentPath: slash <= 0 ? "/" : path.slice(0, slash)
    readonly property string parentShown: parentPath.startsWith(home + "/") ? "~" + parentPath.slice(home.length)
                                                                             : parentPath

    Layout.fillWidth: true
    implicitHeight: rowContent.implicitHeight + Theme.menu.padding * 4
    radius: Theme.menu.itemRadius
    color: rowHover.hovered ? Theme.controls.hover : Qt.alpha(Theme.controls.hover, 0)
    Behavior on color { enabled: Motion.fades; MotionColor {} }
    HoverHandler { id: rowHover }

    RowLayout {
        id: rowContent
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 4
        spacing: Theme.controls.controlSpacing

        Icon {
            path: folderRow.steam ? Icons.steam : Icons.folder
            size: Theme.controls.iconSize
            color: Theme.controls.inputText
            opacity: folderRow.exists ? 1 : Theme.gameEditor.readOnlyOpacity
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            opacity: folderRow.exists ? 1 : Theme.gameEditor.readOnlyOpacity

            Label {
                Layout.fillWidth: true
                text: folderRow.steam ? qsTr("Steam")
                                      : folderRow.path.slice(folderRow.slash + 1) || folderRow.path
                color: Theme.controls.inputText
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                text: folderRow.steam ? qsTr("Built in") : folderRow.parentShown
                color: Theme.controls.descriptionColor
                font.family: folderRow.steam ? font.family : Theme.font.monoFamily
                font.pixelSize: folderRow.steam ? Theme.controls.descriptionFontSize
                                                : Theme.gameEditor.pathFontSize
                elide: Text.ElideMiddle
            }
        }
        Label {
            visible: !folderRow.exists
            text: qsTr("Not found")
            color: Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
        }
        ListButton {
            flat: true
            iconPath: Icons.folderOpenLine
            enabled: folderRow.exists
            textColor: hovered ? foreground : Theme.controls.descriptionColor
            tip: qsTr("Open the folder")
            onClicked: folderRow.openClicked()
        }
        ListButton {
            flat: true
            enabled: !folderRow.steam
            opacity: folderRow.steam ? 0 : 1
            iconPath: Icons.close
            textColor: hovered ? foreground : Theme.controls.descriptionColor
            tip: qsTr("Stop showing this folder")
            onClicked: folderRow.removeClicked()
        }
    }
}
