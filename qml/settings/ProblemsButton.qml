import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    property var problems: []

    visible: problems.length > 0
    Layout.preferredWidth: problemsLabel.implicitWidth + 20
    Layout.preferredHeight: Theme.settingsWindow.toolbarHeight
    radius: Theme.settingsWindow.inputRadius
    color: Theme.settingsWindow.problemsBackground

    Label {
        id: problemsLabel
        anchors.centerIn: parent
        text: qsTr("%n problem(s)", "", root.problems.length)
        color: Theme.settingsWindow.problemsText
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: problemsPopup.visible = !problemsPopup.visible
    }

    MotionPopup {
        id: problemsPopup
        y: parent.height + 4
        x: parent.width - width
        width: 420
        padding: 12
        background: Rectangle {
            color: Theme.settingsWindow.problemsBackground
            radius: Theme.settingsWindow.inputRadius
        }
        contentItem: ColumnLayout {
            spacing: 4
            Repeater {
                model: root.problems
                RowLayout {
                    id: row
                    required property string modelData
                    readonly property string file: GameRunner.problemFile(modelData)
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: row.modelData
                        color: Theme.settingsWindow.problemsText
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.settingsWindow.descriptionFontSize
                    }
                    IconButton {
                        visible: row.file !== ""
                        Layout.alignment: Qt.AlignVCenter
                        plain: true
                        implicitWidth: 24
                        implicitHeight: 24
                        iconSize: 18
                        iconColor: Theme.settingsWindow.problemsText
                        icon: Icons.openExternal
                        tip: qsTr("Open the file")
                        onClicked: GameRunner.openFile(row.file)
                    }
                }
            }
        }
    }
}
