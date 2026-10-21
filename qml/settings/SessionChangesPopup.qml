import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    signal showSetting(string section, string key)

    function title(name) {
        const text = name.replace(/_/g, " ")
        return text.charAt(0).toUpperCase() + text.slice(1)
    }

    width: 520
    padding: Theme.menu.dialogPadding
    // The button that opens it toggles it; a press on it must not close and reopen it.
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    Connections {
        target: Theme
        function onSessionChangesChanged() { if (Theme.sessionChanges.length === 0) root.close() }
    }

    dim: true
    Overlay.modeless: Scrim { color: Theme.menu.popupScrim }

    background: PopoverBackground {}

    ColumnLayout {
        width: parent.width
        spacing: Theme.controls.controlSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.controls.controlSpacing

            Label {
                Layout.fillWidth: true
                text: qsTr("Changed since Settings opened")
                color: Theme.controls.labelColor
                font.weight: Theme.font.strongWeight
            }
            SettingsButton {
                text: qsTr("Undo all")
                onClicked: Theme.revertAll()
            }
        }

        ListView {
            id: changes
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 360)
            clip: true
            model: Theme.sessionChanges
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar {}

            delegate: Rectangle {
                id: change

                required property var modelData
                required property int index

                width: changes.width
                implicitHeight: changeRow.implicitHeight + Theme.settingsWindow.rowPadding * 2
                radius: Theme.settingsWindow.rowRadius
                color: changeHover.hovered ? Theme.controls.hover : "transparent"

                HoverHandler { id: changeHover }

                Rectangle {
                    visible: Theme.settingsWindow.rowDividers && change.index > 0
                    width: parent.width
                    height: 1
                    color: Theme.settingsWindow.rowDivider
                }

                RowLayout {
                    id: changeRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Theme.settingsWindow.rowPadding
                    anchors.rightMargin: 0
                    spacing: Theme.settingsWindow.rowLabelSpacing

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: root.title(change.modelData.key)
                            color: nameHover.hovered ? Theme.settingsWindow.resetButtonHoverColor
                                                     : Theme.controls.labelColor
                            font.pixelSize: Theme.controls.labelFontSize
                            font.underline: nameHover.hovered

                            HoverHandler { id: nameHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                onTapped: {
                                    root.showSetting(change.modelData.section, change.modelData.key)
                                    root.close()
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: root.title(change.modelData.section) + " · "
                                  + (change.modelData.half === "colors" ? qsTr("Colors") : qsTr("Style"))
                            color: Theme.controls.descriptionColor
                            font.pixelSize: Theme.controls.descriptionFontSize
                        }
                    }

                    Label {
                        Layout.maximumWidth: 260
                        elide: Text.ElideMiddle
                        text: "%1  →  %2".arg(change.modelData.before).arg(change.modelData.after)
                        color: Theme.controls.labelColor
                        font.pixelSize: Theme.controls.labelFontSize
                    }

                    SettingsButton {
                        text: qsTr("Undo")
                        tip: qsTr("Back to %1").arg(change.modelData.before)
                        onClicked: Theme.revertValue(change.modelData.section, change.modelData.key)
                    }
                }
            }
        }
    }
}
